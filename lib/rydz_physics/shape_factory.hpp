#pragma once

#include "components.hpp"
#include "jolt_math.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

namespace physics {

inline auto build_shape(Collider const& collider) -> JPH::RefConst<JPH::Shape> {
  return std::visit(
    [&](auto const& s) -> JPH::RefConst<JPH::Shape> {
      using S = std::decay_t<decltype(s)>;

      if constexpr (std::is_same_v<S, Collider::Box>) {
        JPH::BoxShapeSettings settings(to_jph(s.half_extents));
        settings.mDensity = collider.density;
        auto result = settings.Create();
        return result.Get();

      } else if constexpr (std::is_same_v<S, Collider::Sphere>) {
        JPH::SphereShapeSettings settings(s.radius);
        settings.mDensity = collider.density;
        auto result = settings.Create();
        return result.Get();

      } else if constexpr (std::is_same_v<S, Collider::Capsule>) {
        JPH::CapsuleShapeSettings settings(s.half_height, s.radius);
        settings.mDensity = collider.density;
        auto result = settings.Create();
        return result.Get();

      } else {
        static_assert(sizeof(S) == 0, "Unhandled Collider::Shape variant");
        return nullptr;
      }
    },
    collider.shape
  );
}

} // namespace physics
