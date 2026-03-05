#pragma once
#include "asset.hpp"
#include "camera3d.hpp"
#include "mesh3d.hpp"
#include "transform.hpp"
#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <meshoptimizer.h>
#include <raylib.h>
#include <raymath.h>
#include <vector>

namespace ecs {

constexpr int LOD_LEVELS = 4;

struct LodConfig {
  /// Minimum vertex count before LOD levels are generated.
  int vertex_threshold = 500;
  /// Simplification ratios per level (fraction of original).
  std::array<float, LOD_LEVELS> ratios = {1.0f, 0.5f, 0.25f, 0.1f};
  /// Screen-space radius (px) threshold for each LOD level.
  std::array<float, LOD_LEVELS> screen_radius_px = {200.0f, 100.0f, 50.0f,
                                                    15.0f};
  /// Hysteresis band (px) to prevent LOD flickering.
  float hysteresis_px = 8.0f;
  /// Reference triangle count for complexity bias.
  float reference_triangles = 2000.0f;
  float min_complexity_scale = 0.5f;
  float max_complexity_scale = 2.5f;
};

/// Component: stores a Model handle per LOD level.
struct MeshLodGroup {
  std::array<Handle<Model>, LOD_LEVELS> levels{};
  int level_count = 0;
};

/// Resource: per-entity LOD history for hysteresis.
struct LodHistory {
  absl::flat_hash_map<Entity, int, std::hash<Entity>> map;
};

// ====== LOD mesh generation via meshoptimizer ======

/// Simplify a raylib Mesh to a target ratio. Returns a new Mesh.
/// The original mesh must have indices. If it doesn't, returns a copy.
inline Mesh simplify_mesh(const Mesh &src, float target_ratio) {
  if (!src.vertices || src.vertexCount <= 0)
    return src;

  // Build index buffer if not present
  std::vector<unsigned int> indices;
  if (src.indices) {
    indices.resize(src.triangleCount * 3);
    for (int i = 0; i < src.triangleCount * 3; ++i)
      indices[i] = src.indices[i];
  } else {
    indices.resize(src.vertexCount);
    for (int i = 0; i < src.vertexCount; ++i)
      indices[i] = static_cast<unsigned int>(i);
  }

  size_t target_count =
      std::max(static_cast<size_t>(3),
               static_cast<size_t>(indices.size() * target_ratio));

  std::vector<unsigned int> simplified(indices.size());
  float result_error = 0.0f;
  size_t new_count =
      meshopt_simplify(simplified.data(), indices.data(), indices.size(),
                       src.vertices, src.vertexCount, sizeof(float) * 3,
                       target_count, 0.01f, 0, &result_error);
  simplified.resize(new_count);

  if (new_count == 0)
    return src;

  // Remap vertices to compact buffer
  std::vector<unsigned int> remap(src.vertexCount, UINT32_MAX);
  unsigned int new_vertex_count = 0;
  for (auto idx : simplified) {
    if (remap[idx] == UINT32_MAX) {
      remap[idx] = new_vertex_count++;
    }
  }

  // Build new mesh
  Mesh out = {};
  out.vertexCount = static_cast<int>(new_vertex_count);
  out.triangleCount = static_cast<int>(simplified.size() / 3);

  out.vertices =
      static_cast<float *>(RL_CALLOC(new_vertex_count * 3, sizeof(float)));

  if (src.normals)
    out.normals =
        static_cast<float *>(RL_CALLOC(new_vertex_count * 3, sizeof(float)));

  if (src.texcoords)
    out.texcoords =
        static_cast<float *>(RL_CALLOC(new_vertex_count * 2, sizeof(float)));

  // Copy remapped vertex data
  for (int i = 0; i < src.vertexCount; ++i) {
    if (remap[i] == UINT32_MAX)
      continue;
    unsigned int dst = remap[i];
    out.vertices[dst * 3 + 0] = src.vertices[i * 3 + 0];
    out.vertices[dst * 3 + 1] = src.vertices[i * 3 + 1];
    out.vertices[dst * 3 + 2] = src.vertices[i * 3 + 2];

    if (src.normals && out.normals) {
      out.normals[dst * 3 + 0] = src.normals[i * 3 + 0];
      out.normals[dst * 3 + 1] = src.normals[i * 3 + 1];
      out.normals[dst * 3 + 2] = src.normals[i * 3 + 2];
    }

    if (src.texcoords && out.texcoords) {
      out.texcoords[dst * 2 + 0] = src.texcoords[i * 2 + 0];
      out.texcoords[dst * 2 + 1] = src.texcoords[i * 2 + 1];
    }
  }

  // Write remapped indices
  out.indices = static_cast<unsigned short *>(
      RL_CALLOC(simplified.size(), sizeof(unsigned short)));
  for (size_t i = 0; i < simplified.size(); ++i) {
    out.indices[i] = static_cast<unsigned short>(remap[simplified[i]]);
  }

  // Upload to GPU
  UploadMesh(&out, false);
  return out;
}

// ====== LOD selection ======

inline float apply_complexity_bias(float screen_radius_px, float triangle_count,
                                   const LodConfig &config) {
  float tris = std::max(triangle_count, 1.0f);
  float scale = std::sqrt(config.reference_triangles / tris);
  scale = std::clamp(scale, config.min_complexity_scale,
                     config.max_complexity_scale);
  return screen_radius_px * scale;
}

/// Returns LOD level index, or -1 if the object is too small to render.
inline int select_lod_level(float screen_radius_px, const LodConfig &config) {
  for (int level = 0; level < LOD_LEVELS; ++level) {
    if (screen_radius_px >= config.screen_radius_px[level])
      return level;
  }
  return -1; // Too small — cull
}

inline int apply_lod_hysteresis(Entity entity, int desired_level,
                                float screen_radius_px, const LodConfig &config,
                                LodHistory &history) {
  float hysteresis = std::max(config.hysteresis_px, 0.0f);
  auto it = history.map.find(entity);
  int prev = (it != history.map.end()) ? it->second : -1;

  int resolved;
  if (desired_level < 0 && prev >= 0) {
    float threshold = config.screen_radius_px[prev];
    resolved = (screen_radius_px < threshold - hysteresis) ? -1 : prev;
  } else if (desired_level >= 0 && prev < 0) {
    resolved = desired_level;
  } else if (desired_level >= 0 && prev >= 0) {
    if (desired_level == prev) {
      resolved = prev;
    } else if (desired_level < prev) {
      float threshold = config.screen_radius_px[desired_level];
      resolved =
          (screen_radius_px >= threshold + hysteresis) ? desired_level : prev;
    } else {
      float threshold = config.screen_radius_px[prev];
      resolved =
          (screen_radius_px < threshold - hysteresis) ? desired_level : prev;
    }
  } else {
    resolved = -1;
  }

  if (resolved >= 0)
    history.map[entity] = resolved;
  else
    history.map.erase(entity);

  return resolved;
}

// ====== LOD Systems ======

/// Auto-generates LOD meshes for entities with large meshes.
/// Runs on main thread (needs GL for UploadMesh).
/// Deduplicates by model handle ID — LODs generated once per unique model.
inline void auto_generate_lods_system(World &world) {
  auto *mesh_storage = world.get_vec_storage<Mesh3d>();
  auto *lod_storage = world.get_vec_storage<MeshLodGroup>();
  auto *model_assets = world.get_resource<Assets<Model>>();
  auto *config = world.get_resource<LodConfig>();
  if (!mesh_storage || !model_assets || !config)
    return;

  // Cache: model handle id → already-computed MeshLodGroup
  absl::flat_hash_map<uint64_t, MeshLodGroup> lod_cache;

  for (auto e : mesh_storage->entity_indices()) {
    if (lod_storage && lod_storage->get(e))
      continue;

    auto *mesh3d = mesh_storage->get(e);
    if (!mesh3d)
      continue;

    // Check cache first
    auto cache_it = lod_cache.find(mesh3d->model.id);
    if (cache_it != lod_cache.end()) {
      world.insert_component(e, cache_it->second);
      lod_storage = world.get_vec_storage<MeshLodGroup>();
      continue;
    }

    Model *model = model_assets->get(mesh3d->model);
    if (!model || model->meshCount <= 0 || !model->meshes)
      continue;

    // Count total vertices
    int total_verts = 0;
    for (int i = 0; i < model->meshCount; ++i)
      total_verts += model->meshes[i].vertexCount;

    if (total_verts < config->vertex_threshold)
      continue;

    MeshLodGroup group{};
    group.levels[0] = mesh3d->model; // LOD 0 = original
    group.level_count = 1;

    for (int lod = 1; lod < LOD_LEVELS; ++lod) {
      Model lod_model = {};
      lod_model.meshCount = model->meshCount;
      lod_model.meshes =
          static_cast<Mesh *>(RL_CALLOC(model->meshCount, sizeof(Mesh)));

      bool any_valid = false;
      for (int mi = 0; mi < model->meshCount; ++mi) {
        lod_model.meshes[mi] =
            simplify_mesh(model->meshes[mi], config->ratios[lod]);
        if (lod_model.meshes[mi].vertexCount > 0)
          any_valid = true;
      }

      if (any_valid) {
        lod_model.materialCount = model->materialCount;
        lod_model.materials = model->materials;
        lod_model.meshMaterial = model->meshMaterial;
        lod_model.transform = model->transform;

        group.levels[lod] = model_assets->add(std::move(lod_model));
        group.level_count = lod + 1;
      }
    }

    lod_cache[mesh3d->model.id] = group;
    world.insert_component(e, group);
    lod_storage = world.get_vec_storage<MeshLodGroup>();
  }
}

/// Compute the screen-space pixel radius for a bounding sphere.
inline float compute_screen_radius(Vector3 center, float radius,
                                   const Matrix &view, const Matrix &proj,
                                   float half_screen_height) {
  Vector3 view_pos = Vector3Transform(center, view);
  float depth = -view_pos.z;
  float effective_depth = std::max(depth, 0.1f);

  float proj_scale_y = proj.m5; // projection[1][1]
  float radius_ndc = (radius / effective_depth) * proj_scale_y;
  float radius_px = std::abs(radius_ndc) * half_screen_height;

  return std::isfinite(radius_px) ? radius_px : 0.0f;
}

} // namespace ecs
