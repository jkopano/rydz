#pragma once

#include "layers.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_physics/contact_listener.hpp"
#include "types.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <cassert>
#include <memory>
#include <thread>
#include <vector>

namespace physics {

struct PhysicsContactListener;

inline auto jolt_init_once() -> void {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;

  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
}

struct PhysicsSettings {
  using T = ecs::Resource;

  f32 fixed_hz = 60.0f;
  f32 gravity_y = -9.81f;
  u32 max_bodies = 4096;
  u32 num_body_mutexes = 0;
  u32 max_body_pairs = 65536;
  u32 max_contact_constraints = 10240;
  u32 num_threads = 0;
  usize temp_allocator_size_mb = 10;
};

struct PhysicsInterp {
  using T = ecs::Resource;
  f32 alpha = 0.0f;
};

struct PhysicsWorld {
  using T = ecs::Resource;

  std::unique_ptr<JPH::PhysicsSystem> system;
  std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
  std::unique_ptr<JPH::JobSystemThreadPool> job_system;

  std::unique_ptr<BPLayerInterfaceImpl> bp_layer_interface;
  std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> obj_layer_pair_filter;
  std::unique_ptr<BroadPhaseCanCollideImpl> bp_can_collide;

  std::unique_ptr<PhysicsContactListener> contact_listener;

  std::vector<JPH::BodyID> pending_remove;

  PhysicsWorld() = default;

  explicit PhysicsWorld(PhysicsSettings const& s) {
    jolt_init_once();

    temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(
      s.temp_allocator_size_mb * 1024 * 1024
    );

    u32 const threads =
      s.num_threads != 0
        ? s.num_threads
        : std::max(1u, std::thread::hardware_concurrency() - 2);
    job_system = std::make_unique<JPH::JobSystemThreadPool>(
      JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(threads)
    );

    bp_layer_interface = std::make_unique<BPLayerInterfaceImpl>();
    obj_layer_pair_filter =
      std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    bp_can_collide = std::make_unique<BroadPhaseCanCollideImpl>();

    system = std::make_unique<JPH::PhysicsSystem>();
    system->Init(
      s.max_bodies,
      s.num_body_mutexes,
      s.max_body_pairs,
      s.max_contact_constraints,
      *bp_layer_interface,
      *bp_can_collide,
      *obj_layer_pair_filter
    );

    system->SetGravity(JPH::Vec3(0.0f, s.gravity_y, 0.0f));
  }

  ~PhysicsWorld() = default;

  PhysicsWorld(PhysicsWorld const&) = delete;
  auto operator=(PhysicsWorld const&) -> PhysicsWorld& = delete;
  PhysicsWorld(PhysicsWorld&&) = default;
  auto operator=(PhysicsWorld&&) -> PhysicsWorld& = default;

  [[nodiscard]] auto body_interface() -> JPH::BodyInterface& {
    assert(system);
    return system->GetBodyInterface();
  }

  [[nodiscard]] auto body_interface() const -> JPH::BodyInterface const& {
    assert(system);
    return system->GetBodyInterface();
  }
};

} // namespace physics
