#pragma once
/*
 * rydz_camera - Isometric camera system for rydz ECS engine
 * 
 * Provides IsometricCamera component, controller system, and setup helper.
 * Single header for top-down games and RPGs.
 */

#include "math.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/resource.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/camera3d.hpp"
#include "rydz_graphics/transform.hpp"

namespace ecs {

using namespace math;

// ── Isometric Camera Component ──────────────────────────────────────────────

struct IsometricCamera {
  using Storage = SparseSetStorage<IsometricCamera>;
  
  Vec3 target = Vec3::sZero();       // Follow target position
  Vec3 offset = Vec3(10.0f, 10.0f, 10.0f);  // Camera offset from target
  
  f32 follow_speed = 5.0f;           // Smooth follow lerp speed
  bool smooth_follow = true;         // Enable/disable smooth following
};

// ── Isometric Camera System ─────────────────────────────────────────────────

inline void isometric_camera_system(
    Query<Mut<Transform>, IsometricCamera> query,
    Res<Time> time) {
    
  for (auto [t, cam] : query.iter()) {
    f32 dt = time->delta_seconds;
    
    // Compute desired position from target and offset
    Vec3 desired = cam->target + cam->offset;
    
    // Smooth follow if enabled
    if (cam->smooth_follow) {
      t->translation = t->translation.Lerp(desired, std::min(cam->follow_speed * dt, 1.0f));
    } else {
      t->translation = desired;
    }
    
    t->look_at(cam->target);
  }
}

// ── Setup Helper ────────────────────────────────────────────────────────────

struct IsometricCameraSetup {
  Vec3 target = Vec3::sZero();
  Vec3 offset = Vec3(10.0f, 10.0f, 10.0f);
  f32 ortho_height = 20.0f;
  f32 follow_speed = 5.0f;
};

inline void setup_isometric_camera(
    Cmd cmd,
    const Vec3& target = Vec3::sZero(),
    const Vec3& offset = Vec3(10.0f, 10.0f, 10.0f),
    f32 ortho_height = 20.0f,
    f32 follow_speed = 5.0f) {
  
  cmd.spawn(
    Camera3DComponent::orthographic(ortho_height),
    ActiveCamera{},
    Transform::from_xyz(target.x + offset.x, target.y + offset.y, target.z + offset.z)
      .look_at(target),
    IsometricCamera{
      .target = target,
      .offset = offset,
      .follow_speed = follow_speed
    }
  );
}

} // namespace ecs
