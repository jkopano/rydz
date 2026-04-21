#pragma once
/*
 * rydz_camera - Isometric camera system for rydz ECS engine
 *
 * Provides IsometricCamera component, controller system, and setup helper.
 * Single header for top-down games and RPGs.
 */

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_console/command_api.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/plugin.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include <algorithm>
#include <cmath>

namespace ecs {

using namespace math;

struct IsometricCamera {
  using Storage = SparseSetStorage<IsometricCamera>;

  Vec3 target = Vec3::ZERO;
  Vec3 offset = Vec3(10.0f, 10.0f, 10.0f);

  f32 follow_speed = 5.0f;
  bool smooth_follow = true;
};

inline void isometric_camera_system(
  Query<Mut<Transform>, IsometricCamera> query, Res<Time> time
) {

  for (auto [t, cam] : query.iter()) {
    f32 dt = time->delta_seconds;

    Vec3 desired = cam->target + cam->offset;

    if (cam->smooth_follow) {
      // Exponential smoothing gives stable feel across different frame rates.
      f32 alpha = 1.0f - std::exp(-cam->follow_speed * dt);
      t->translation = t->translation + (desired - t->translation) * alpha;
    } else {
      t->translation = desired;
    }

    t->look_at(cam->target);
  }
}

struct IsometricCameraBundle {
  using T = Bundle;
  Camera3d camera_component;
  ActiveCamera active_camera;
  Transform transform;
  IsometricCamera iso;

  static auto setup(
    Vec3 target = Vec3::ZERO,
    Vec3 offset = Vec3(10.0f, 10.0f, 10.0f),
    f32 ortho_height = 20.0f,
    f32 follow_speed = 5.0f
  ) -> IsometricCameraBundle {
    return IsometricCameraBundle{
      .camera_component = Camera3d::orthographic(ortho_height),
      .active_camera = ActiveCamera{},
      .transform =
        Transform::from_xyz(
          target.x + offset.x, target.y + offset.y, target.z + offset.z
        )
          .look_at(target),
      .iso = IsometricCamera{
        .target = target, .offset = offset, .follow_speed = follow_speed
      }
    };
  }
};

inline auto camera_plugin(App& app) -> void {
  app.add_systems(ScheduleLabel::Update, isometric_camera_system);

  app.add_systems(ScheduleLabel::Startup, [](World& world) -> void {
    engine::BindCommand<float>::to(world, "set_zoom", [](float zoom_level) {
      return [zoom_level](Query<Mut<Camera3d>> query) -> void {
        for (auto [cam] : query.iter()) {
          if (cam->is_orthographic()) {
            cam->orthographic_height = zoom_level;
          }
        }
      };
    });

    engine::BindCommand<float>::to(world, "set_cam_speed", [](float speed) {
      return [speed](Query<Mut<IsometricCamera>> query) -> void {
        for (auto [iso_cam] : query.iter()) {
          iso_cam->follow_speed = speed;
        }
      };
    });
  });
}

} // namespace ecs
