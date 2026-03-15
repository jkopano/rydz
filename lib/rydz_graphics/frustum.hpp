#pragma once
#include "camera3d.hpp"
#include "mesh3d.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"
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

inline bool is_aabb_in_frustum(const BBox3 &bbox,
                               const std::array<FrustumPlane, 6> &planes) {
  for (const auto &plane : planes) {
    rl::Vector3 p_vertex = {
        plane.normal.x >= 0 ? bbox.max[0] : bbox.min[0],
        plane.normal.y >= 0 ? bbox.max[1] : bbox.min[1],
        plane.normal.z >= 0 ? bbox.max[2] : bbox.min[2],
    };
    float dist = plane.normal.x * p_vertex.x + plane.normal.y * p_vertex.y +
                 plane.normal.z * p_vertex.z + plane.distance;
    if (dist < 0)
      return false;
  }
  return true;
}

inline std::array<FrustumPlane, 6>
extract_frustum_planes(const rl::Matrix &vp) {
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

struct MeshBounds {
  BBox3 bbox = BBox3::make_empty();
};

inline void
compute_mesh_bounds_system(Query<Entity, Model3d, Without<MeshBounds>> query,
                           Res<Assets<rl::Model>> model_assets, Cmd cmd) {
  for (auto [e, model3d] : query.iter()) {
    const rl::Model *model = model_assets->get(model3d->model);
    if (!model || model->meshCount <= 0 || !model->meshes)
      continue;

    auto combined = BBox3::make_empty();
    for (int i = 0; i < model->meshCount; ++i)
      combined.extend(compute_local_bbox(model->meshes[i]));

    cmd.entity(e).insert(MeshBounds{.bbox = combined});
  }
}

inline void frustum_cull_system(
    Cmd cmd,
    Query<Camera3DComponent, Transform3D, With<ActiveCamera>> cam_query,
    Query<Entity, MeshBounds, GlobalTransform, Opt<ComputedVisibility>>
        mesh_query) {
  auto cam_result = cam_query.single();
  if (!cam_result)
    return;
  auto [cam_comp, cam_tx] = *cam_result;

  rl::Camera3D cam = build_camera(*cam_tx, *cam_comp);
  rl::Matrix view = rl::GetCameraViewMatrix(&cam);
  rl::Matrix projection = rl::GetCameraProjectionMatrix(
      &cam, static_cast<float>(rl::GetScreenWidth()) /
                static_cast<float>(std::max(rl::GetScreenHeight(), 1)));
  rl::Matrix vp = rl::MatrixMultiply(view, projection);
  auto planes = extract_frustum_planes(vp);

  struct CullEntry {
    Entity entity;
    const MeshBounds *bounds;
    const GlobalTransform *gt;
    const ComputedVisibility *cv;
  };

  std::vector<CullEntry> entries;
  for (auto [e, mb, gt, cv] : mesh_query.iter())
    entries.push_back({e, mb, gt, cv});

  if (entries.empty())
    return;

  std::vector<ViewVisibility> results(entries.size());

  tf::Taskflow taskflow;
  taskflow.for_each_index(
      size_t{0}, entries.size(), size_t{1}, [&](size_t idx) {
        auto &entry = entries[idx];

        if (entry.cv && !entry.cv->visible) {
          results[idx] = ViewVisibility{false};
          return;
        }

        BBox3 world_bbox = transform_bbox(entry.bounds->bbox, entry.gt->matrix);
        results[idx] = ViewVisibility{is_aabb_in_frustum(world_bbox, planes)};
      });

  static tf::Executor executor;
  executor.run(taskflow).wait();

  for (size_t i = 0; i < entries.size(); ++i)
    cmd.entity(entries[i].entity).insert(results[i]);
}

} // namespace ecs
