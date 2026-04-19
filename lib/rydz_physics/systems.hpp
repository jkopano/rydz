#pragma once

#include "components.hpp"
#include "contact_listener.hpp"
#include "jolt_math.hpp"
#include "physics_world.hpp"
#include "rydz_log/mod.hpp"
#include "shape_factory.hpp"

#include "rydz_ecs/mod.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

namespace physics {

using namespace ecs;
using namespace rydz_math;

inline auto to_jph_motion(MotionType t) -> JPH::EMotionType {
  switch (t) {
  case MotionType::Static:
    return JPH::EMotionType::Static;
  case MotionType::Kinematic:
    return JPH::EMotionType::Kinematic;
  case MotionType::Dynamic:
    return JPH::EMotionType::Dynamic;
  }
  return JPH::EMotionType::Dynamic;
}

inline void store_previous_transforms(
  Query<Transform const, Mut<PreviousTransform>, MovingBody const> query
) {
  for (auto [transform, prev, moving] : query.iter()) {
    prev->translation = transform->translation;
    prev->rotation = transform->rotation;
  }
}

inline void flush_body_despawn_queue(ResMut<PhysicsWorld> pw) {
  if (pw->pending_remove.empty()) {
    return;
  }

  auto& bi = pw->body_interface();
  for (auto body_id : pw->pending_remove) {
    bi.RemoveBody(body_id);
    bi.DestroyBody(body_id);
  }
  pw->pending_remove.clear();
}

inline void flush_body_spawn_queue(
  Query<
    Entity,
    RigidBody const,
    Collider const,
    Opt<Transform const>,
    Filters<Without<PhysicsHandle>>> query,
  ResMut<PhysicsWorld> pw,
  Cmd cmd
) {
  if (pw->system == nullptr) {
    return;
  }

  auto& bi = pw->body_interface();

  for (auto [e, rb, col, opt_tr] : query.iter()) {
    auto shape = build_shape(*col);
    if (shape == nullptr) {
      continue;
    }

    Vec3 pos = Vec3::ZERO;
    Quat rot = Quat::IDENTITY;
    if (opt_tr != nullptr) {
      pos = opt_tr->translation;
      rot = opt_tr->rotation;
    }

    JPH::BodyCreationSettings bcs(
      shape,
      to_jph_r(pos),
      to_jph(rot),
      to_jph_motion(rb->motion_type),
      rb->collision_layer
    );
    bcs.mLinearDamping = rb->linear_damping;
    bcs.mAngularDamping = rb->angular_damping;
    bcs.mRestitution = col->restitution;
    bcs.mFriction = col->friction;
    bcs.mIsSensor = col->is_sensor;

    JPH::BodyID body_id = bi.CreateAndAddBody(
      bcs,
      rb->start_active ? JPH::EActivation::Activate
                       : JPH::EActivation::DontActivate
    );

    auto e_cmd = cmd.entity(e);
    e_cmd.insert(PhysicsHandle{body_id});

    if (rb->motion_type != MotionType::Static) {
      e_cmd.insert(PreviousTransform{.translation = pos, .rotation = rot});
      e_cmd.insert(MovingBody{});

      if (rb->motion_type == MotionType::Dynamic) {
        e_cmd.insert(DynamicBody{});
        e_cmd.insert(LinearVelocity{});
        e_cmd.insert(AngularVelocity{});
      }
    }
  }
}

inline void sync_kinematic_to_jolt(
  Query<PhysicsHandle const, Transform const, RigidBody const, MovingBody const>
    query,
  ResMut<PhysicsWorld> pw,
  Res<FixedTime> fixed_time
) {
  auto& bi = pw->body_interface();

  for (auto [handle, transform, rb, moving] : query.iter()) {
    if (rb->motion_type != MotionType::Kinematic) {
      continue;
    }

    bi.MoveKinematic(
      handle->body_id,
      to_jph_r(transform->translation),
      to_jph(transform->rotation),
      fixed_time->step()
    );
  }
}

inline void apply_forces(
  Query<Entity, PhysicsHandle const, ExternalForce const> force_query,
  Query<Entity, PhysicsHandle const, ExternalImpulse const> impulse_query,
  ResMut<PhysicsWorld> pw,
  Cmd cmd
) {
  auto& bi = pw->body_interface();

  for (auto [e, handle, ext] : force_query.iter()) {
    if (ext->force.length_sq() > 0.0f) {
      bi.AddForce(handle->body_id, to_jph(ext->force));
    }
    if (ext->torque.length_sq() > 0.0f) {
      bi.AddTorque(handle->body_id, to_jph(ext->torque));
    }
    cmd.entity(e).remove<ExternalForce>();
  }

  for (auto [e, handle, ext] : impulse_query.iter()) {
    if (ext->impulse.length_sq() > 0.0f) {
      bi.AddImpulse(handle->body_id, to_jph(ext->impulse));
    }
    if (ext->angular_impulse.length_sq() > 0.0f) {
      bi.AddAngularImpulse(handle->body_id, to_jph(ext->angular_impulse));
    }
    cmd.entity(e).remove<ExternalImpulse>();
  }
}

inline void step_simulation(ResMut<PhysicsWorld> pw, Res<FixedTime> fixed) {
  if (pw->system == nullptr) {
    return;
  }

  pw->system->Update(
    fixed->step(), 1, pw->temp_allocator.get(), pw->job_system.get()
  );
}

inline void sync_transforms(
  Query<PhysicsHandle const, Mut<Transform>, DynamicBody const> query,
  Res<PhysicsWorld> pw
) {
  auto const& bi = pw->body_interface();

  for (auto [handle, transform, dyn] : query.iter()) {
    if (!bi.IsActive(handle->body_id)) {
      continue;
    }

    JPH::RVec3 pos = bi.GetPosition(handle->body_id);
    JPH::Quat rot = bi.GetRotation(handle->body_id);

    transform->translation = from_jph(pos);
    transform->rotation = from_jph(rot);
  }
}

inline void sync_velocities(
  Query<
    PhysicsHandle const,
    Mut<LinearVelocity>,
    Mut<AngularVelocity>,
    DynamicBody const> query,
  Res<PhysicsWorld> pw
) {
  auto const& bi = pw->body_interface();

  for (auto [handle, lin_vel, ang_vel, dyn] : query.iter()) {
    if (!bi.IsActive(handle->body_id)) {
      continue;
    }

    lin_vel->value = from_jph(bi.GetLinearVelocity(handle->body_id));
    ang_vel->value = from_jph(bi.GetAngularVelocity(handle->body_id));
  }
}

inline void process_contact_events(
  ResMut<PhysicsWorld> pw, MessageWriter<ContactEvent> writer
) {
  if (pw->contact_listener == nullptr) {
    return;
  }

  auto events = pw->contact_listener->drain();
  for (auto& event : events) {
    writer.send(std::move(event));
  }
}

} // namespace physics
