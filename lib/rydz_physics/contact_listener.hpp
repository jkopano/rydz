#pragma once

#include "rydz_ecs/fwd.hpp"
#include "types.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/ContactListener.h>

#include <mutex>
#include <vector>

namespace physics {

struct ContactEvent {
  using T = ecs::Message;
  enum class Type : u8 { Begin, End };
  Type type;
  JPH::BodyID body_a;
  JPH::BodyID body_b;
};

struct PhysicsContactListener : JPH::ContactListener {
  std::mutex mutex_;
  std::vector<ContactEvent> pending_;

  auto OnContactValidate(
    [[maybe_unused]] JPH::Body const& body1,
    [[maybe_unused]] JPH::Body const& body2,
    [[maybe_unused]] JPH::RVec3Arg base_offset,
    [[maybe_unused]] JPH::CollideShapeResult const& collision_result
  ) -> JPH::ValidateResult override {
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
  }

  void OnContactAdded(
    JPH::Body const& body1,
    JPH::Body const& body2,
    [[maybe_unused]] JPH::ContactManifold const& manifold,
    [[maybe_unused]] JPH::ContactSettings& settings
  ) override {
    std::lock_guard lock(mutex_);
    pending_.push_back(
      {ContactEvent::Type::Begin, body1.GetID(), body2.GetID()}
    );
  }

  void OnContactRemoved(JPH::SubShapeIDPair const& pair) override {
    std::lock_guard lock(mutex_);
    pending_.push_back(
      {ContactEvent::Type::End, pair.GetBody1ID(), pair.GetBody2ID()}
    );
  }

  auto drain() -> std::vector<ContactEvent> {
    std::lock_guard lock(mutex_);
    auto result = std::move(pending_);
    pending_.clear();
    return result;
  }
};

} // namespace physics
