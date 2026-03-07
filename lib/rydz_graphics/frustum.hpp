#pragma once
#include "rydz_ecs/asset.hpp"
#include "camera3d.hpp"
#include "mesh3d.hpp"
#include "rydz_ecs/transform.hpp"
#include "rydz_ecs/visibility.hpp"
#include <array>
#include <bvh/v2/bbox.h>
#include <bvh/v2/vec.h>
#include <raylib.h>
#include <raymath.h>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace ecs {

using BVec3 = bvh::v2::Vec<float, 3>;
using BBox3 = bvh::v2::BBox<float, 3>;

struct FrustumPlane {
  Vector3 normal = {0, 0, 0};
  float distance = 0.0f;
};

struct ViewVisibility {
  bool visible = true;
};

inline BVec3 to_bvec(Vector3 v) { return BVec3(v.x, v.y, v.z); }
inline Vector3 to_rl(BVec3 v) { return {v[0], v[1], v[2]}; }

inline BBox3 compute_local_bbox(const Mesh &mesh) {
  if (!mesh.vertices || mesh.vertexCount <= 0)
    return BBox3::make_empty();

  auto bbox = BBox3::make_empty();
  for (int i = 0; i < mesh.vertexCount; ++i) {
    bbox.extend(BVec3(mesh.vertices[i * 3 + 0], mesh.vertices[i * 3 + 1],
                      mesh.vertices[i * 3 + 2]));
  }
  return bbox;
}

inline BBox3 transform_bbox(const BBox3 &local, const Matrix &m) {
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
    Vector3 p = Vector3Transform({c[0], c[1], c[2]}, m);
    result.extend(BVec3(p.x, p.y, p.z));
  }
  return result;
}

inline float bbox_radius(const BBox3 &bbox) {
  auto d = bbox.get_diagonal();
  return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) * 0.5f;
}

inline std::array<FrustumPlane, 6> extract_frustum_planes(const Matrix &vp) {
  auto row = [&](int i) -> Vector4 {
    const float *base = &vp.m0;
    int off = i * 4;
    return {base[off], base[off + 1], base[off + 2], base[off + 3]};
  };

  Vector4 r0 = row(0);
  Vector4 r1 = row(1);
  Vector4 r2 = row(2);
  Vector4 r3 = row(3);

  auto add4 = [](Vector4 a, Vector4 b) -> Vector4 {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
  };
  auto sub4 = [](Vector4 a, Vector4 b) -> Vector4 {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
  };

  auto normalize_plane = [](Vector4 v) -> FrustumPlane {
    Vector3 n = {v.x, v.y, v.z};
    float len = Vector3Length(n);
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

inline bool sphere_in_frustum(Vector3 center, float radius,
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
  auto *model_assets = world.get_resource<Assets<Model>>();
  if (!mesh_storage || !model_assets)
    return;

  for (auto e : mesh_storage->entity_indices()) {
    if (bounds_storage && bounds_storage->get(e))
      continue;

    auto *mesh3d = mesh_storage->get(e);
    if (!mesh3d)
      continue;

    Model *model = model_assets->get(mesh3d->model);
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
  for (auto e : active_storage->entity_indices()) {
    cam_comp = cam_storage->get(e);
    cam_tx = cam_tx_storage->get(e);
    if (cam_comp && cam_tx)
      break;
  }
  if (!cam_comp || !cam_tx)
    return;

  int sw = GetScreenWidth();
  int sh = std::max(GetScreenHeight(), 1);
  double aspect = static_cast<double>(sw) / static_cast<double>(sh);
  Matrix projection =
      MatrixPerspective(cam_comp->fov * DEG2RAD, aspect, cam_comp->near_plane,
                        cam_comp->far_plane);
  Camera3D cam = build_camera(cam_tx->translation, cam_tx->forward(),
                              cam_tx->up(), *cam_comp);
  Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
  Matrix vp = MatrixMultiply(view, projection);
  auto planes = extract_frustum_planes(vp);

  auto *bounds_storage = world.get_storage<MeshBounds>();
  auto *gt_storage = world.get_storage<GlobalTransform>();
  auto *cv_storage = world.get_storage<ComputedVisibility>();
  if (!bounds_storage || !gt_storage)
    return;

  auto entities = bounds_storage->entity_indices();
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
        auto c = world_bbox.get_center();
        Vector3 center = {c[0], c[1], c[2]};
        float radius = bbox_radius(world_bbox);

        results[idx] = ViewVisibility{
            sphere_in_frustum(center, radius, planes)};
        has_result[idx] = 1;
      });

  static tf::Executor executor;
  executor.run(taskflow).wait();

  for (size_t i = 0; i < entities.size(); ++i) {
    if (has_result[i])
      world.insert_component(entities[i], results[i]);
  }
}

} // namespace ecs
