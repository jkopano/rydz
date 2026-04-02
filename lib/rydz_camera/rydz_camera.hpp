#pragma once
/*
 * rydz_camera - Isometric camera system for rydz ECS engine
 *
 * Provides IsometricCamera component, controller system, and setup helper.
 * Single header for top-down games and RPGs.
 */

#include "math.hpp"
#include "rl.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_camera/rydz_camera.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include "rydz_graphics/transform.hpp"
#include <algorithm>
#include <cmath>

namespace ecs {

using namespace math;

struct IsometricCamera {
  using Storage = SparseSetStorage<IsometricCamera>;

  Vec3 target = Vec3::sZero();
  Vec3 offset = Vec3(10.0f, 10.0f, 10.0f);

  f32 follow_speed = 5.0f;
  bool smooth_follow = true;
};

inline void
isometric_camera_system(Query<Mut<Transform>, IsometricCamera> query,
                        Res<Time> time) {

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
  Camera3DComponent camera_component;
  ActiveCamera active_camera;
  Transform transform;
  IsometricCamera iso;

  static IsometricCameraBundle setup(Vec3 target = Vec3::sZero(),
                                     Vec3 offset = Vec3(10.0f, 10.0f, 10.0f),
                                     f32 ortho_height = 20.0f,
                                     f32 follow_speed = 5.0f) {
    return IsometricCameraBundle{
        .camera_component = Camera3DComponent::orthographic(ortho_height),
        .active_camera = ActiveCamera{},
        .transform = Transform::from_xyz(target.GetX() + offset.GetX(),
                                         target.GetY() + offset.GetY(),
                                         target.GetZ() + offset.GetZ())
                         .look_at(target),
        .iso = IsometricCamera{
            .target = target, .offset = offset, .follow_speed = follow_speed}};
  }
};

} // namespace ecs
