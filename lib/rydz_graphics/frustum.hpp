#pragma once
#include "math.hpp"
#include "mesh3d.hpp"
#include "rl.hpp"
#include "rydz_camera/camera3d.hpp"
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
  auto row = [&](int i) -> Vec4 {
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
    Query<Camera3DComponent, GlobalTransform, With<ActiveCamera>> cam_query,
    Query<Entity, MeshBounds, GlobalTransform, Opt<ComputedVisibility>>
        mesh_query,
    Cmd cmd) {
  auto cam_result = cam_query.single();
  if (!cam_result)
    return;
  auto [cam_comp, cam_gt] = *cam_result;

  CameraView cam_view = compute_camera_view(*cam_gt, *cam_comp);
  Mat4 view = cam_view.view;
  Mat4 proj = cam_view.proj;
  Mat4 vp = proj * view;
  auto planes = extract_frustum_planes(vp);

  static int dbg_frame = 0;
  bool do_debug = (dbg_frame++ % 120 == 0);
  if (do_debug) {
    // Compare Jolt VP rows vs round-tripped through raylib Matrix
    rl::Matrix rl_vp = to_rl(vp);
    const float *base = &rl_vp.m0;
    for (int i = 0; i < 4; ++i) {
      Vec4 jolt_row(vp(i, 0), vp(i, 1), vp(i, 2), vp(i, 3));
      // Memory layout: base[i*4+0..3] = row i of raylib matrix
      Vec4 rl_row(base[i * 4], base[i * 4 + 1], base[i * 4 + 2],
                  base[i * 4 + 3]);
      Vec4 diff = jolt_row - rl_row;
      if (diff.Length() > 1e-6f) {
        rl::TraceLog(LOG_WARNING,
                     "VP ROW %d MISMATCH! jolt=(%.6f,%.6f,%.6f,%.6f) "
                     "rl=(%.6f,%.6f,%.6f,%.6f)",
                     i, jolt_row.GetX(), jolt_row.GetY(), jolt_row.GetZ(),
                     jolt_row.GetW(), rl_row.GetX(), rl_row.GetY(),
                     rl_row.GetZ(), rl_row.GetW());
      }
    }

    Vec3 fwd = cam_gt->forward();
    rl::TraceLog(LOG_DEBUG, "CAM pos=(%.2f,%.2f,%.2f) fwd=(%.2f,%.2f,%.2f)",
                 cam_gt->translation().GetX(), cam_gt->translation().GetY(),
                 cam_gt->translation().GetZ(), fwd.GetX(), fwd.GetY(),
                 fwd.GetZ());
    for (int pi = 0; pi < 6; ++pi) {
      const char *names[] = {"LEFT", "RIGHT", "BOTTOM", "TOP", "NEAR", "FAR"};
      rl::TraceLog(LOG_DEBUG, "  plane[%s] n=(%.3f,%.3f,%.3f) d=%.3f",
                   names[pi], planes[pi].normal.GetX(),
                   planes[pi].normal.GetY(), planes[pi].normal.GetZ(),
                   planes[pi].distance);
    }
  }

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

  if (do_debug) {
    int vis = 0, cull = 0, false_cull = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
      if (results[i].visible) {
        vis++;
      } else {
        cull++;
        AABox wb =
            transform_bbox(entries[i].bounds->bbox, entries[i].gt->matrix);
        // Check ALL 8 corners of the world AABB against clip space
        Vec3 corners[8] = {
            Vec3(wb.mMin.GetX(), wb.mMin.GetY(), wb.mMin.GetZ()),
            Vec3(wb.mMin.GetX(), wb.mMin.GetY(), wb.mMax.GetZ()),
            Vec3(wb.mMin.GetX(), wb.mMax.GetY(), wb.mMin.GetZ()),
            Vec3(wb.mMin.GetX(), wb.mMax.GetY(), wb.mMax.GetZ()),
            Vec3(wb.mMax.GetX(), wb.mMin.GetY(), wb.mMin.GetZ()),
            Vec3(wb.mMax.GetX(), wb.mMin.GetY(), wb.mMax.GetZ()),
            Vec3(wb.mMax.GetX(), wb.mMax.GetY(), wb.mMin.GetZ()),
            Vec3(wb.mMax.GetX(), wb.mMax.GetY(), wb.mMax.GetZ()),
        };
        bool any_corner_onscreen = false;
        for (auto &c : corners) {
          Vec4 clip = vp * Vec4(c, 1.0f);
          float w = clip.GetW();
          if (w > 0.0f) {
            float nx = clip.GetX() / w;
            float ny = clip.GetY() / w;
            float nz = clip.GetZ() / w;
            if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f &&
                nz >= -1.0f && nz <= 1.0f) {
              any_corner_onscreen = true;
              break;
            }
          }
        }
        if (any_corner_onscreen && false_cull < 10) {
          false_cull++;
          Vec3 center = (wb.mMin + wb.mMax) * 0.5f;
          Vec3 extent = (wb.mMax - wb.mMin) * 0.5f;
          // Find failing plane
          const char *names[] = {"LEFT", "RIGHT", "BOTTOM",
                                 "TOP",  "NEAR",  "FAR"};
          float worst = -1e30f;
          int worst_pi = -1;
          for (int pi = 0; pi < 6; ++pi) {
            Vec3 p(
                planes[pi].normal.GetX() >= 0 ? wb.mMax.GetX() : wb.mMin.GetX(),
                planes[pi].normal.GetY() >= 0 ? wb.mMax.GetY() : wb.mMin.GetY(),
                planes[pi].normal.GetZ() >= 0 ? wb.mMax.GetZ()
                                              : wb.mMin.GetZ());
            float d = planes[pi].normal.Dot(p) + planes[pi].distance;
            if (d < 0 && d > worst) {
              worst = d;
              worst_pi = pi;
            }
          }
          rl::TraceLog(LOG_WARNING,
                       "FALSE CULL! center=(%.2f,%.2f,%.2f) "
                       "extent=(%.2f,%.2f,%.2f) "
                       "plane=%s dist=%.4f",
                       center.GetX(), center.GetY(), center.GetZ(),
                       extent.GetX(), extent.GetY(), extent.GetZ(),
                       names[worst_pi], worst);
        }
      }
    }
    rl::TraceLog(LOG_DEBUG, "  visible=%d culled=%d false_cull=%d total=%d",
                 vis, cull, false_cull, (int)entries.size());
  }

  for (size_t i = 0; i < entries.size(); ++i)
    cmd.entity(entries[i].entity).insert(results[i]);
}

} // namespace ecs
