#pragma once

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mesh3d_component.hpp"
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
  Vec3 normal = Vec3::ZERO;
  float distance = 0.0F;
};

struct ViewVisibility {
  bool visible = true;
};

inline auto compute_local_bbox(gl::Mesh const& mesh) -> AABox {
  if ((mesh.vertex_data() == nullptr) || mesh.vertex_count() <= 0) {
    return {};
  }

  AABox bbox;
  bbox.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  bbox.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (int i = 0; i < mesh.vertex_count(); ++i) {
    float const* vertices = mesh.vertex_data();
    Vec3 vec(
      vertices[(i * 3) + 0], vertices[(i * 3) + 1], vertices[(i * 3) + 2]
    );
    bbox.encapsulate(vec);
  }
  return bbox;
}

inline auto transform_bbox(AABox const& local, Mat4 m) -> AABox {
  std::array<Vec3, 8> corners{
    Vec3(local.mMin.x, local.mMin.y, local.mMin.z),
    Vec3(local.mMin.x, local.mMin.y, local.mMax.z),
    Vec3(local.mMin.x, local.mMax.y, local.mMin.z),
    Vec3(local.mMin.x, local.mMax.y, local.mMax.z),
    Vec3(local.mMax.x, local.mMin.y, local.mMin.z),
    Vec3(local.mMax.x, local.mMin.y, local.mMax.z),
    Vec3(local.mMax.x, local.mMax.y, local.mMin.z),
    Vec3(local.mMax.x, local.mMax.y, local.mMax.z),
  };

  AABox result;
  result.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  result.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  for (auto& corner : corners) {
    result.encapsulate(m * corner);
  }
  return result;
}

inline auto aabb_in_frustum(
  AABox const& bbox, std::array<FrustumPlane, 6> const& planes
) -> bool {
  return std::ranges::all_of(planes, [&](auto const& plane) -> auto {
    Vec3 const p_vertex(
      plane.normal.x >= 0 ? bbox.mMax.x : bbox.mMin.x,
      plane.normal.y >= 0 ? bbox.mMax.y : bbox.mMin.y,
      plane.normal.z >= 0 ? bbox.mMax.z : bbox.mMin.z
    );

    return (plane.normal.dot(p_vertex) + plane.distance) >= 0;
  });
}

inline auto extract_frustum_planes(Mat4 vp) -> std::array<FrustumPlane, 6> {
  auto row = [&](int i) -> Vec4 {
    return {vp(i, 0), vp(i, 1), vp(i, 2), vp(i, 3)};
  };

  Vec4 r0 = row(0);
  Vec4 r1 = row(1);
  Vec4 r2 = row(2);
  Vec4 r3 = row(3);

  auto normalize_plane = [](Vec4 v) -> FrustumPlane {
    Vec3 n(v.x, v.y, v.z);
    float len = n.length();
    if (len > 1e-8F) {
      return {.normal = n / len, .distance = v.w / len};
    }
    return {};
  };

  return {
    normalize_plane(r3 + r0),
    normalize_plane(r3 - r0),
    normalize_plane(r3 + r1),
    normalize_plane(r3 - r1),
    normalize_plane(r3 + r2),
    normalize_plane(r3 - r2),
  };
}

struct MeshBounds {
  AABox bbox;
};

inline auto compute_mesh_bounds_system(
  Query<Entity, Mesh3d, Without<MeshBounds>> query,
  Res<Assets<Mesh>> mesh_assets,
  Cmd cmd
) -> void {
  for (auto [e, mesh3d] : query.iter()) {
    auto const* mesh = mesh_assets->get(mesh3d->mesh);
    if ((mesh == nullptr) || mesh->vertex_count() <= 0 ||
        (mesh->vertex_data() == nullptr)) {
      continue;
    }

    cmd.entity(e).insert(MeshBounds{.bbox = compute_local_bbox(*mesh)});
  }
}

inline auto frustum_cull_system(
  Query<Camera3d, GlobalTransform, With<ActiveCamera>> cam_query,
  Query<Entity, MeshBounds, GlobalTransform, Opt<ComputedVisibility>>
    mesh_query,
  Res<Window> window,
  Cmd cmd
) -> void {
  auto cam_result = cam_query.single();
  if (!cam_result) {
    return;
  }
  auto [cam_comp, cam_gt] = *cam_result;
  float const aspect = compute_camera_aspect_ratio(
    static_cast<float>(window->width), static_cast<float>(window->height)
  );

  CameraView cam_view = compute_camera_view(*cam_gt, *cam_comp, aspect);
  Mat4 view = cam_view.view;
  Mat4 proj = cam_view.proj;
  Mat4 vp = proj * view;
  auto planes = extract_frustum_planes(vp);

  struct CullEntry {
    Entity entity;
    MeshBounds const* bounds;
    GlobalTransform const* gt;
    ComputedVisibility const* cv;
  };

  std::vector<CullEntry> entries;
  for (auto [e, mb, gt, cv] : mesh_query.iter()) {
    entries.push_back({e, mb, gt, cv});
  }

  if (entries.empty()) {
    return;
  }

  std::vector<ViewVisibility> results(entries.size());

  tf::Taskflow taskflow;
  taskflow.for_each_index(
    size_t{0}, entries.size(), size_t{1}, [&](size_t idx) -> void {
      auto& entry = entries[idx];

      if (entry.cv && !entry.cv->visible) {
        results[idx] = ViewVisibility{false};
        return;
      }

      AABox world_bbox = transform_bbox(entry.bounds->bbox, entry.gt->matrix);
      results[idx] = ViewVisibility{aabb_in_frustum(world_bbox, planes)};
    }
  );

  static tf::Executor executor;
  executor.run(taskflow).wait();

  for (size_t i = 0; i < entries.size(); ++i) {
    cmd.entity(entries[i].entity).insert(results[i]);
  }
}

} // namespace ecs
