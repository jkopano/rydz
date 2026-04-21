#pragma once

#include "types.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace physics {

namespace Layers {
inline constexpr JPH::ObjectLayer NON_MOVING = 0;
inline constexpr JPH::ObjectLayer MOVING = 1;
inline constexpr JPH::ObjectLayer TRIGGER = 2;
inline constexpr JPH::ObjectLayer NUM_LAYERS = 3;
} // namespace Layers

namespace BPLayers {
inline constexpr JPH::BroadPhaseLayer NON_MOVING{0};
inline constexpr JPH::BroadPhaseLayer MOVING{1};
inline constexpr u32 NUM_LAYERS = 2;
} // namespace BPLayers

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
  [[nodiscard]] auto GetNumBroadPhaseLayers() const -> u32 override {
    return BPLayers::NUM_LAYERS;
  }

  [[nodiscard]] auto GetBroadPhaseLayer(JPH::ObjectLayer layer) const
    -> JPH::BroadPhaseLayer override {
    switch (layer) {
    case Layers::NON_MOVING:
      return BPLayers::NON_MOVING;
    case Layers::MOVING:
      return BPLayers::MOVING;
    case Layers::TRIGGER:
      return BPLayers::MOVING;
    default:
      return BPLayers::MOVING;
    }
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  [[nodiscard]] auto GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
    -> char const* override {
    switch (static_cast<u8>(layer)) {
    case 0:
      return "NON_MOVING";
    case 1:
      return "MOVING";
    default:
      return "UNKNOWN";
    }
  }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final
    : public JPH::ObjectLayerPairFilter {
public:
  [[nodiscard]] auto ShouldCollide(
    JPH::ObjectLayer layer1, JPH::ObjectLayer layer2
  ) const -> bool override {
    switch (layer1) {
    case Layers::NON_MOVING:
      return layer2 == Layers::MOVING || layer2 == Layers::TRIGGER;
    case Layers::MOVING:
      return true; // MOVING collides with everything
    case Layers::TRIGGER:
      return layer2 == Layers::MOVING;
    default:
      return false;
    }
  }
};

class BroadPhaseCanCollideImpl final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  [[nodiscard]] auto ShouldCollide(
    JPH::ObjectLayer layer, JPH::BroadPhaseLayer bp
  ) const -> bool override {
    switch (layer) {
    case Layers::NON_MOVING:
      return bp == BPLayers::MOVING;
    case Layers::MOVING:
      return true;
    case Layers::TRIGGER:
      return bp == BPLayers::MOVING;
    default:
      return false;
    }
  }
};

} // namespace physics
