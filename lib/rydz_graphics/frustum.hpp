#pragma once
#include "camera3d.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"
#include "types.hpp"
#include <array>
#include <cfloat>
#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace ecs {

using namespace math;

struct FrustumPlane {
  Vec3 normal = Vec3::sZero();
  float distance = 0.0f;
};

struct ViewVisibility {
  bool visible = true;
};

inline AABox compute_local_bbox(const rl::Mesh &mesh) {
  if (!mesh.vertices || mesh.vertexCount <= 0)
    return AABox();

  AABox bbox;
  bbox.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  bbox.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (int i = 0; i < mesh.vertexCount; ++i) {
    Vec3 v(mesh.vertices[i * 3 + 0], mesh.vertices[i * 3 + 1],
           mesh.vertices[i * 3 + 2]);
    bbox.Encapsulate(v);
  }
  return bbox;
}

inline AABox transform_bbox(const AABox &local, Mat4 m) {
  Vec3 corners[8] = {
      Vec3(local.mMin.GetX(), local.mMin.GetY(), local.mMin.GetZ()),
      Vec3(local.mMin.GetX(), local.mMin.GetY(), local.mMax.GetZ()),
      Vec3(local.mMin.GetX(), local.mMax.GetY(), local.mMin.GetZ()),
      Vec3(local.mMin.GetX(), local.mMax.GetY(), local.mMax.GetZ()),
      Vec3(local.mMax.GetX(), local.mMin.GetY(), local.mMin.GetZ()),
      Vec3(local.mMax.GetX(), local.mMin.GetY(), local.mMax.GetZ()),
      Vec3(local.mMax.GetX(), local.mMax.GetY(), local.mMin.GetZ()),
      Vec3(local.mMax.GetX(), local.mMax.GetY(), local.mMax.GetZ()),
  };

  AABox result;
  result.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  result.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (auto &c : corners) {
    Vec3 p = m * c;
    result.Encapsulate(p);
  }
  return result;
}

inline bool aabb_in_frustum(const AABox &bbox,
                            const std::array<FrustumPlane, 6> &planes) {
  for (const auto &plane : planes) {
    Vec3 p_vertex(
        plane.normal.GetX() >= 0 ? bbox.mMax.GetX() : bbox.mMin.GetX(),
        plane.normal.GetY() >= 0 ? bbox.mMax.GetY() : bbox.mMin.GetY(),
        plane.normal.GetZ() >= 0 ? bbox.mMax.GetZ() : bbox.mMin.GetZ());
    float dist = plane.normal.Dot(p_vertex) + plane.distance;
    if (dist < 0)
      return false;
  }
  return true;
}

inline std::array<FrustumPlane, 6> extract_frustum_planes(Mat4 vp) {
  auto row = [&](u32 i) -> Vec4 {
    return Vec4(vp(i, 0), vp(i, 1), vp(i, 2), vp(i, 3));
  };

  Vec4 r0 = row(0);
  Vec4 r1 = row(1);
  Vec4 r2 = row(2);
  Vec4 r3 = row(3);

  auto normalize_plane = [](Vec4 v) -> FrustumPlane {
    Vec3 n(v.GetX(), v.GetY(), v.GetZ());
    float len = n.Length();
    if (len > 1e-8f)
      return {n / len, v.GetW() / len};
    return {};
  };

  return {
      normalize_plane(r3 + r0), // Left
      normalize_plane(r3 - r0), // Right
      normalize_plane(r3 + r1), // Bottom
      normalize_plane(r3 - r1), // Top
      normalize_plane(r3 + r2), // Near
      normalize_plane(r3 - r2), // Far
  };
}

struct MeshBounds {
  AABox bbox;
};

inline void
compute_mesh_bounds_system(Query<Entity, Model3d, Without<MeshBounds>> query,
                           Res<Assets<rl::Model>> model_assets, Cmd cmd) {
  for (auto [e, model3d] : query.iter()) {
    const rl::Model *model = model_assets->get(model3d->model);
    if (!model || model->meshCount <= 0 || !model->meshes)
      continue;

    AABox combined;
    combined.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    combined.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < model->meshCount; ++i) {
      AABox mesh_bbox = compute_local_bbox(model->meshes[i]);
      combined.Encapsulate(mesh_bbox);
    }

    cmd.entity(e).insert(MeshBounds{.bbox = combined});
  }
}

inline void frustum_cull_system(
    Query<Camera3DComponent, Transform3D, With<ActiveCamera>> cam_query,
    Query<Entity, MeshBounds, GlobalTransform, Opt<ComputedVisibility>>
        mesh_query,
    Cmd cmd) {
  auto cam_result = cam_query.single();
  if (!cam_result)
    return;
  auto [cam_comp, cam_tx] = *cam_result;

  float aspect = static_cast<float>(rl::GetScreenWidth()) /
                 static_cast<float>(std::max(rl::GetScreenHeight(), 1));
  float fov_rad = static_cast<float>(cam_comp->fov) * DEG2RAD;

  Mat4 view =
      Mat4::sLookAt(cam_tx->translation,
                    cam_tx->translation + cam_tx->forward(), cam_tx->up());
  Mat4 proj = Mat4::sPerspective(fov_rad, aspect,
                                 static_cast<float>(cam_comp->near_plane),
                                 static_cast<float>(cam_comp->far_plane));
  Mat4 vp = proj * view;
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

        AABox world_bbox = transform_bbox(entry.bounds->bbox, entry.gt->matrix);
        results[idx] = ViewVisibility{aabb_in_frustum(world_bbox, planes)};
      });

  static tf::Executor executor;
  executor.run(taskflow).wait();

  for (size_t i = 0; i < entries.size(); ++i)
    cmd.entity(entries[i].entity).insert(results[i]);
}

} // namespace ecs
