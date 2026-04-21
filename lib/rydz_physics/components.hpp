#pragma once

#include "layers.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_math/mod.hpp"
#include "types.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>

#include <variant>

namespace physics {

using namespace rydz_math;

enum class MotionType : u8 { Static, Kinematic, Dynamic };

struct RigidBody {
  using T = ecs::Component;
  MotionType motion_type = MotionType::Dynamic;
  u8 collision_layer = Layers::MOVING;
  bool start_active = true;
  f32 linear_damping = 0.05f;
  f32 angular_damping = 0.05f;
};

struct Collider {
  using T = ecs::Component;

  struct Box {
    Vec3 half_extents = Vec3::splat(0.5f);
  };
  struct Sphere {
    f32 radius = 0.5f;
  };
  struct Capsule {
    f32 half_height = 0.5f;
    f32 radius = 0.25f;
  };

  using Shape = std::variant<Box, Sphere, Capsule>;
  Shape shape = Box{};

  f32 restitution = 0.0f;
  f32 friction = 0.6f;
  f32 density = 1000.0f;
  bool is_sensor = false;
};

struct LinearVelocity {
  using T = ecs::Component;
  Vec3 value{};
};

struct AngularVelocity {
  using T = ecs::Component;
  Vec3 value{};
};

struct ExternalForce {
  using T = ecs::Component;
  Vec3 force{};
  Vec3 torque{};
};

struct ExternalImpulse {
  using T = ecs::Component;
  Vec3 impulse{};
  Vec3 angular_impulse{};
};

struct PhysicsHandle {
  using T = ecs::Component;
  JPH::BodyID body_id;
};

struct PreviousTransform : ecs::Component {
  Vec3 translation{};
  Quat rotation = Quat::IDENTITY;
};

struct Sleeping : ecs::Component {};
struct Grounded : ecs::Component {};
struct MovingBody : ecs::Component {}; 
struct DynamicBody : ecs::Component {};

} // namespace physics
