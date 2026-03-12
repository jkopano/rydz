#pragma once
#include "camera3d.hpp"
#include "mesh3d.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/transform.hpp"
#include "rydz_ecs/visibility.hpp"
#include "rl.hpp"
#include <array>
#include <bvh/v2/bbox.h>
#include <bvh/v2/vec.h>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace ecs {

using BVec3 = bvh::v2::Vec<float, 3>;
using BBox3 = bvh::v2::BBox<float, 3>;

struct FrustumPlane {
  rl::Vector3 normal = {0, 0, 0};
  float distance = 0.0f;
};

struct ViewVisibility {
  bool visible = true;
};

inline BVec3 to_bvec(rl::Vector3 v) { return BVec3(v.x, v.y, v.z); }
inline rl::Vector3 to_rl(BVec3 v) { return {v[0], v[1], v[2]}; }

inline BBox3 compute_local_bbox(const rl::Mesh &mesh) {
  if (!mesh.vertices || mesh.vertexCount <= 0)
    return BBox3::make_empty();

  auto bbox = BBox3::make_empty();
  for (int i = 0; i < mesh.vertexCount; ++i) {
    bbox.extend(BVec3(mesh.vertices[i * 3 + 0], mesh.vertices[i * 3 + 1],
                      mesh.vertices[i * 3 + 2]));
  }
  return bbox;
}

inline BBox3 transform_bbox(const BBox3 &local, const rl::Matrix &m) {
  BVec3 corners[8] = {
      {local.min[0], local.min[1], local.min[2]},
      {local.min[0], local.min[1], local.max[2]},
      {local.min[0], local.max[1], local.min[2]},
      {local.min[0], local.max[1], local.max[2]},
      {local.max[0], local.min[1], local.min[2]},
      {local.max[0], local.min[1], local.max[2]},
      {local.max[0], local.max[1], local.min[2]},
      {local.max[0], local.max[1], local.max[2]},
  };

  auto result = BBox3::make_empty();
  for (auto &c : corners) {
    rl::Vector3 p = rl::Vector3Transform({c[0], c[1], c[2]}, m);
    result.extend(BVec3(p.x, p.y, p.z));
  }
  return result;
}

inline bool aabb_in_frustum(const BBox3 &bbox,
                            const std::array<FrustumPlane, 6> &planes) {
  for (const auto &plane : planes) {
    // p-vertex: the corner of the AABB most in the direction of the plane
    // normal
    rl::Vector3 p_vertex = {
        plane.normal.x >= 0 ? bbox.max[0] : bbox.min[0],
        plane.normal.y >= 0 ? bbox.max[1] : bbox.min[1],
        plane.normal.z >= 0 ? bbox.max[2] : bbox.min[2],
    };
    float dist = plane.normal.x * p_vertex.x + plane.normal.y * p_vertex.y +
                 plane.normal.z * p_vertex.z + plane.distance;
    if (dist < 0)
      return false; // entire box is behind this plane
  }
  return true;
}

inline std::array<FrustumPlane, 6> extract_frustum_planes(const rl::Matrix &vp) {
  auto row = [&](int i) -> rl::Vector4 {
    const float *base = &vp.m0;
    int off = i * 4;
    return {base[off], base[off + 1], base[off + 2], base[off + 3]};
  };

  rl::Vector4 r0 = row(0);
  rl::Vector4 r1 = row(1);
  rl::Vector4 r2 = row(2);
  rl::Vector4 r3 = row(3);

  auto add4 = [](rl::Vector4 a, rl::Vector4 b) -> rl::Vector4 {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
  };
  auto sub4 = [](rl::Vector4 a, rl::Vector4 b) -> rl::Vector4 {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
  };

  auto normalize_plane = [](rl::Vector4 v) -> FrustumPlane {
    rl::Vector3 n = {v.x, v.y, v.z};
    float len = rl::Vector3Length(n);
    if (len > 1e-8f)
      return {{n.x / len, n.y / len, n.z / len}, v.w / len};
    return {};
  };

  return {
      normalize_plane(add4(r3, r0)), // Left
      normalize_plane(sub4(r3, r0)), // Right
      normalize_plane(add4(r3, r1)), // Bottom
      normalize_plane(sub4(r3, r1)), // Top
      normalize_plane(add4(r3, r2)), // Near
      normalize_plane(sub4(r3, r2)), // Far
  };
}

inline bool sphere_in_frustum(rl::Vector3 center, float radius,
                              const std::array<FrustumPlane, 6> &planes) {
  for (const auto &plane : planes) {
    float dist = plane.normal.x * center.x + plane.normal.y * center.y +
                 plane.normal.z * center.z + plane.distance;
    if (dist < -radius)
      return false;
  }
  return true;
}

struct MeshBounds {
  BBox3 bbox = BBox3::make_empty();
};
inline void compute_mesh_bounds_system(World &world) {
  auto *mesh_storage = world.get_storage<Mesh3d>();
  auto *bounds_storage = world.get_storage<MeshBounds>();
  auto *model_assets = world.get_resource<Assets<rl::Model>>();
  if (!mesh_storage || !model_assets)
    return;

  for (auto e : mesh_storage->entities()) {
    if (bounds_storage && bounds_storage->get(e))
      continue;

    auto *mesh3d = mesh_storage->get(e);
    if (!mesh3d)
      continue;

    rl::Model *model = model_assets->get(mesh3d->model);
    if (!model || model->meshCount <= 0 || !model->meshes)
      continue;

    auto combined = BBox3::make_empty();
    for (int i = 0; i < model->meshCount; ++i) {
      combined.extend(compute_local_bbox(model->meshes[i]));
    }

    world.insert_component(e, MeshBounds{combined});
    bounds_storage = world.get_storage<MeshBounds>();
  }
}

inline void frustum_cull_system(World &world) {
  auto *cam_storage = world.get_storage<Camera3DComponent>();
  auto *active_storage = world.get_storage<ActiveCamera>();
  auto *cam_tx_storage = world.get_storage<Transform3D>();
  if (!cam_storage || !active_storage || !cam_tx_storage)
    return;

  const Camera3DComponent *cam_comp = nullptr;
  const Transform3D *cam_tx = nullptr;
  for (auto e : active_storage->entities()) {
    cam_comp = cam_storage->get(e);
    cam_tx = cam_tx_storage->get(e);
    if (cam_comp && cam_tx)
      break;
  }
  if (!cam_comp || !cam_tx)
    return;

  int sw = rl::GetScreenWidth();
  int sh = std::max(rl::GetScreenHeight(), 1);
  double aspect = static_cast<double>(sw) / static_cast<double>(sh);
  rl::Matrix projection =
      rl::MatrixPerspective(cam_comp->fov * DEG2RAD, aspect, cam_comp->near_plane,
                        cam_comp->far_plane);
  rl::Camera3D cam = build_camera(cam_tx->translation, cam_tx->forward(),
                              cam_tx->up(), *cam_comp);
  rl::Matrix view = rl::MatrixLookAt(cam.position, cam.target, cam.up);
  rl::Matrix vp = rl::MatrixMultiply(view, projection);
  auto planes = extract_frustum_planes(vp);

  auto *bounds_storage = world.get_storage<MeshBounds>();
  auto *gt_storage = world.get_storage<GlobalTransform>();
  auto *cv_storage = world.get_storage<ComputedVisibility>();
  if (!bounds_storage || !gt_storage)
    return;

  auto entities = bounds_storage->entities();
  if (entities.empty())
    return;

  std::vector<ViewVisibility> results(entities.size());
  std::vector<uint8_t> has_result(entities.size(), 0);

  tf::Taskflow taskflow;
  taskflow.for_each_index(
      size_t{0}, entities.size(), size_t{1}, [&](size_t idx) {
        Entity e = entities[idx];
        auto *mb = bounds_storage->get(e);
        auto *gt = gt_storage->get(e);
        if (!mb || !gt)
          return;

        if (cv_storage) {
          auto *cv = cv_storage->get(e);
          if (cv && !cv->visible) {
            results[idx] = ViewVisibility{false};
            has_result[idx] = 1;
            return;
          }
        }

        BBox3 world_bbox = transform_bbox(mb->bbox, gt->matrix);

        results[idx] = ViewVisibility{aabb_in_frustum(world_bbox, planes)};
        has_result[idx] = 1;
      });

  static tf::Executor executor;
  executor.run(taskflow).wait();

  int dbg_visible = 0, dbg_culled = 0;
  for (size_t i = 0; i < entities.size(); ++i) {
    if (has_result[i]) {
      world.insert_component(entities[i], results[i]);
      if (results[i].visible)
        dbg_visible++;
      else
        dbg_culled++;
    }
  }
  rl::TraceLog(LOG_DEBUG, "FRUSTUM: %d visible, %d culled, %d total entities",
           dbg_visible, dbg_culled, (int)entities.size());
}

} // namespace ecs
