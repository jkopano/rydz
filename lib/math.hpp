#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Vec4.h>

#include <raylib.h>

namespace math {

using Vec3 = JPH::Vec3;
using Vec4 = JPH::Vec4;
using Mat4 = JPH::Mat44;
using Quat = JPH::Quat;
using AABox = JPH::AABox;

inline ::Vector3 to_rl(Vec3 v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

inline Vec3 from_rl(::Vector3 v) { return Vec3(v.x, v.y, v.z); }

inline ::Vector4 to_rl(Vec4 v) {
  return {v.GetX(), v.GetY(), v.GetZ(), v.GetW()};
}

inline Vec4 from_rl(::Vector4 v) { return Vec4(v.x, v.y, v.z, v.w); }

inline ::Vector4 to_rl(Quat q) {
  return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
}

inline ::Matrix to_rl(Mat4 m) {
  return {
      m(0, 0), m(0, 1), m(0, 2), m(0, 3), // row 0
      m(1, 0), m(1, 1), m(1, 2), m(1, 3), // row 1
      m(2, 0), m(2, 1), m(2, 2), m(2, 3), // row 2
      m(3, 0), m(3, 1), m(3, 2), m(3, 3), // row 3
  };
}

inline Mat4 from_rl(const ::Matrix &m) {
  return Mat4(Vec4(m.m0, m.m1, m.m2, m.m3),    // col 0
              Vec4(m.m4, m.m5, m.m6, m.m7),    // col 1
              Vec4(m.m8, m.m9, m.m10, m.m11),  // col 2
              Vec4(m.m12, m.m13, m.m14, m.m15) // col 3
  );
}

} // namespace math
