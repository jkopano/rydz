#pragma once

#include "rydz_math/mod.hpp"
#include "types.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

namespace physics {

using namespace rydz_math;

inline auto to_jph(Vec3 const& v) -> JPH::Vec3 { return {v.x, v.y, v.z}; }

inline auto to_jph_r(Vec3 const& v) -> JPH::RVec3 { return {v.x, v.y, v.z}; }

inline auto from_jph(JPH::Vec3Arg v) -> Vec3 {
  return {v.GetX(), v.GetY(), v.GetZ()};
}
#ifdef JPH_DOUBLE_PRECISION
inline auto from_jph(JPH::RVec3Arg v) -> Vec3 {
  return {
    static_cast<f32>(v.GetX()),
    static_cast<f32>(v.GetY()),
    static_cast<f32>(v.GetZ())
  };
}
#endif

inline auto to_jph(Quat const& q) -> JPH::Quat { return {q.x, q.y, q.z, q.w}; }

inline auto from_jph(JPH::QuatArg q) -> Quat {
  return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
}

} // namespace physics
