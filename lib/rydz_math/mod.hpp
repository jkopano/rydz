#pragma once

#include "rydz_ecs/requires.hpp"
#include "types.hpp"
#include <algorithm>
#include <cmath>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <limits>
#include <raylib.h>

namespace rydz_math {

struct Transform;

struct Vec2 : public glm::vec2 {
  using Base = glm::vec2;
  using Base::Base;

  Vec2() : Base(0.0f) {}
  Vec2(const Base &value) : Base(value) {}

  static Vec2 sZero() { return Vec2(0.0f); }
  static Vec2 sReplicate(f32 value) { return Vec2(value); }

  [[nodiscard]] f32 GetX() const { return x; }
  [[nodiscard]] f32 GetY() const { return y; }

  [[nodiscard]] f32 LengthSq() const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] f32 Length() const {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] Vec2 Normalized() const {
    const f32 len = Length();
    if (len <= 1e-8f) {
      return sZero();
    }
    return Vec2(glm::normalize(static_cast<const Base &>(*this)));
  }

  [[nodiscard]] f32 Dot(const Vec2 &other) const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(other));
  }
};

struct Vec3 : public glm::vec3 {
  using Base = glm::vec3;
  using Base::Base;

  Vec3() : Base(0.0f) {}
  Vec3(const Base &value) : Base(value) {}

  static Vec3 sZero() { return Vec3(0.0f); }
  static Vec3 sReplicate(f32 value) { return Vec3(value); }

  [[nodiscard]] f32 GetX() const { return x; }
  [[nodiscard]] f32 GetY() const { return y; }
  [[nodiscard]] f32 GetZ() const { return z; }

  [[nodiscard]] f32 LengthSq() const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] f32 Length() const {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] Vec3 Normalized() const {
    const f32 len = Length();
    if (len <= 1e-8f) {
      return sZero();
    }
    return Vec3(glm::normalize(static_cast<const Base &>(*this)));
  }

  [[nodiscard]] f32 Dot(const Vec3 &other) const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(other));
  }

  [[nodiscard]] Vec3 Cross(const Vec3 &other) const {
    return Vec3(glm::cross(static_cast<const Base &>(*this),
                           static_cast<const Base &>(other)));
  }
};

struct Vec4 : public glm::vec4 {
  using Base = glm::vec4;
  using Base::Base;

  Vec4() : Base(0.0F) {}
  Vec4(const Base &value) : Base(value) {}

  static Vec4 sZero() { return Vec4{0.0F}; }
  static Vec4 sReplicate(f32 value) { return Vec4{value}; }

  [[nodiscard]] f32 GetX() const { return x; }
  [[nodiscard]] f32 GetY() const { return y; }
  [[nodiscard]] f32 GetZ() const { return z; }
  [[nodiscard]] f32 GetW() const { return w; }
};

struct Quat : public glm::quat {
  using Base = glm::quat;

  Quat() : Base(1.0F, 0.0F, 0.0F, 0.0F) {}
  Quat(const Base &value) : Base(value) {}
  Quat(f32 x, f32 y, f32 z, f32 w) : Base(w, x, y, z) {}

  static Quat sIdentity() { return {0.0F, 0.0F, 0.0F, 1.0F}; }

  static Quat sEulerAngles(const Vec3 &angles) {
    return {glm::quat(static_cast<const Vec3::Base &>(angles))};
  }

  [[nodiscard]] f32 GetX() const { return x; }
  [[nodiscard]] f32 GetY() const { return y; }
  [[nodiscard]] f32 GetZ() const { return z; }
  [[nodiscard]] f32 GetW() const { return w; }

  [[nodiscard]] Quat Normalized() const {
    return {glm::normalize(static_cast<const Base &>(*this))};
  }
};

struct Mat4 : public glm::mat4 {
public:
  using Base = glm::mat4;

  Mat4() : Base(1.0F) {}
  Mat4(const Base &value) : Base(value) {}
  explicit Mat4(f32 diagonal) : Base(diagonal) {}
  Mat4(const Vec4 &column0, const Vec4 &column1, const Vec4 &column2,
       const Vec4 &column3)
      : Base(static_cast<const Vec4::Base &>(column0),
             static_cast<const Vec4::Base &>(column1),
             static_cast<const Vec4::Base &>(column2),
             static_cast<const Vec4::Base &>(column3)) {}

  static Mat4 sIdentity() { return Mat4(1.0F); }

  static Mat4 sPerspective(f32 fov_y_radians, f32 aspect, f32 near_plane,
                           f32 far_plane) {
    return {
        glm::perspectiveRH_NO(fov_y_radians, aspect, near_plane, far_plane)};
  }

  static Mat4 sOrthographic(f32 left, f32 right, f32 bottom, f32 top,
                            f32 near_plane, f32 far_plane) {
    return {glm::orthoRH_NO(left, right, bottom, top, near_plane, far_plane)};
  }

  static Mat4 sLookAt(const Vec3 &position, const Vec3 &target,
                      const Vec3 &up) {
    return {glm::lookAtRH(static_cast<const Vec3::Base &>(position),
                          static_cast<const Vec3::Base &>(target),
                          static_cast<const Vec3::Base &>(up))};
  }

  static Mat4 sRotationTranslation(const Quat &rotation,
                                   const Vec3 &translation) {
    glm::mat4 matrix =
        glm::mat4_cast(static_cast<const Quat::Base &>(rotation));
    matrix[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0F);
    return {matrix};
  }

  [[nodiscard]] Mat4 PreScaled(const Vec3 &scale) const {
    return {glm::scale(static_cast<const Base &>(*this),
                       static_cast<const Vec3::Base &>(scale))};
  }

  [[nodiscard]] Mat4 Inversed() const {
    return {glm::inverse(static_cast<const Base &>(*this))};
  }

  void SetTranslation(const Vec3 &translation) {
    (*this)[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0F);
  }

  [[nodiscard]] Quat GetQuaternion() const {
    return (glm::quat_cast(glm::mat3(static_cast<const Base &>(*this))));
  }

  [[nodiscard]] Vec3 GetTranslation() const {
    const glm::vec4 column = (*this)[3];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] Vec3 GetAxisX() const {
    const glm::vec4 column = (*this)[0];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] Vec3 GetAxisY() const {
    const glm::vec4 column = (*this)[1];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] Vec3 GetAxisZ() const {
    const glm::vec4 column = (*this)[2];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] Vec3 Multiply3x3(const Vec3 &value) const {
    return {glm::mat3(static_cast<const Base &>(*this)) *
            static_cast<const Vec3::Base &>(value)};
  }

  [[nodiscard]] f32 operator()(i32 row, i32 column) const {
    return (*this)[column][row];
  }

  [[nodiscard]] f32 &operator()(i32 row, i32 column) {
    return (*this)[column][row];
  }

  [[nodiscard]] Transform decompose() const;
};

struct GlobalTransform;

struct Transform {
  using Required = ecs::Requires<GlobalTransform>;

  Vec3 translation = Vec3::sZero();
  Quat rotation = Quat::sIdentity();
  Vec3 scale = Vec3::sReplicate(1.0F);

  static Transform from_xyz(f32 x, f32 y, f32 z) {
    return Transform{.translation = {Vec3(x, y, z)},
                     .rotation = Quat::sIdentity(),
                     .scale = Vec3::sReplicate(1.0F)};
  }

  static Transform from_translation(Vec3 pos) {
    return Transform{.translation = pos,
                     .rotation = Quat::sIdentity(),
                     .scale = Vec3::sReplicate(1.0F)};
  }

  static Transform from_rotation(Quat q) {
    return Transform{.translation = Vec3::sZero(),
                     .rotation = q,
                     .scale = Vec3::sReplicate(1.0F)};
  }

  static Transform from_scale(Vec3 s) {
    return Transform{.translation = Vec3::sZero(),
                     .rotation = Quat::sIdentity(),
                     .scale = s};
  }

  [[nodiscard]] Mat4 compute_matrix() const {
    return Mat4::sRotationTranslation(rotation, translation).PreScaled(scale);
  }

  [[nodiscard]] Mat4 to_matrix() const { return compute_matrix(); }

  [[nodiscard]] Vec3 forward() const { return rotation * Vec3(0, 0, -1); }

  [[nodiscard]] Vec3 right() const { return rotation * Vec3(1, 0, 0); }

  [[nodiscard]] Vec3 up() const { return rotation * Vec3(0, 1, 0); }

  Transform &look_at(Vec3 target, Vec3 world_up = Vec3(0, 1, 0)) {
    Vec3 dir = target - translation;
    if (dir.LengthSq() < 1e-10F) {
      return *this;
    }

    Mat4 look = Mat4::sLookAt(translation, target, world_up);
    Mat4 inv = look.Inversed();
    inv.SetTranslation(Vec3::sZero());
    rotation = inv.GetQuaternion().Normalized();
    return *this;
  }
};

struct GlobalTransform {
  Mat4 matrix = Mat4::sIdentity();

  static GlobalTransform from_matrix(Mat4 m) { return {m}; }

  [[nodiscard]] Vec3 translation() const { return matrix.GetTranslation(); }
  [[nodiscard]] Vec3 forward() const {
    return Vec3(-matrix.GetAxisZ()).Normalized();
  }

  [[nodiscard]] Vec3 right() const { return matrix.GetAxisX().Normalized(); }
  [[nodiscard]] Vec3 up() const { return matrix.GetAxisY().Normalized(); }
};

inline Transform Mat4::decompose() const {
  glm::vec3 scale{};
  glm::quat rotation{};
  glm::vec3 translation{};
  glm::vec3 skew{};
  glm::vec4 perspective{};

  glm::decompose(static_cast<const Base &>(*this), scale, rotation, translation,
                 skew, perspective);

  return Transform{
      .translation = Vec3(translation),
      .rotation = Quat(rotation).Normalized(),
      .scale = Vec3(scale),
  };
}

inline Vec2 operator+(Vec2 lhs, const Vec2 &rhs) {
  return {static_cast<const Vec2::Base &>(lhs) +
          static_cast<const Vec2::Base &>(rhs)};
}

inline Vec2 operator-(Vec2 lhs, const Vec2 &rhs) {
  return {static_cast<const Vec2::Base &>(lhs) -
          static_cast<const Vec2::Base &>(rhs)};
}

inline Vec2 operator-(const Vec2 &value) {
  return {-static_cast<const Vec2::Base &>(value)};
}

inline Vec2 operator*(Vec2 lhs, f32 rhs) {
  return {static_cast<const Vec2::Base &>(lhs) * rhs};
}

inline Vec2 operator*(f32 lhs, Vec2 rhs) {
  return {lhs * static_cast<const Vec2::Base &>(rhs)};
}

inline Vec2 operator/(Vec2 lhs, f32 rhs) {
  return {static_cast<const Vec2::Base &>(lhs) / rhs};
}

inline Vec3 operator+(Vec3 lhs, const Vec3 &rhs) {
  return {static_cast<const Vec3::Base &>(lhs) +
          static_cast<const Vec3::Base &>(rhs)};
}

inline Vec3 operator-(Vec3 lhs, const Vec3 &rhs) {
  return {static_cast<const Vec3::Base &>(lhs) -
          static_cast<const Vec3::Base &>(rhs)};
}

inline Vec3 operator-(const Vec3 &value) {
  return {-static_cast<const Vec3::Base &>(value)};
}

inline Vec3 operator*(Vec3 lhs, f32 rhs) {
  return {static_cast<const Vec3::Base &>(lhs) * rhs};
}

inline Vec3 operator*(f32 lhs, Vec3 rhs) {
  return {lhs * static_cast<const Vec3::Base &>(rhs)};
}

inline Vec3 operator/(Vec3 lhs, f32 rhs) {
  return {static_cast<const Vec3::Base &>(lhs) / rhs};
}

inline Vec4 operator+(Vec4 lhs, const Vec4 &rhs) {
  return {static_cast<const Vec4::Base &>(lhs) +
          static_cast<const Vec4::Base &>(rhs)};
}

inline Vec4 operator-(Vec4 lhs, const Vec4 &rhs) {
  return {static_cast<const Vec4::Base &>(lhs) -
          static_cast<const Vec4::Base &>(rhs)};
}

inline Vec4 operator-(const Vec4 &value) {
  return {-static_cast<const Vec4::Base &>(value)};
}

inline Vec4 operator*(Vec4 lhs, f32 rhs) {
  return {static_cast<const Vec4::Base &>(lhs) * rhs};
}

inline Vec4 operator*(f32 lhs, Vec4 rhs) {
  return {lhs * static_cast<const Vec4::Base &>(rhs)};
}

inline Vec4 operator/(Vec4 lhs, f32 rhs) {
  return {static_cast<const Vec4::Base &>(lhs) / rhs};
}

inline Quat operator*(const Quat &lhs, const Quat &rhs) {
  return {static_cast<const Quat::Base &>(lhs) *
          static_cast<const Quat::Base &>(rhs)};
}

inline Vec3 operator*(const Quat &rotation, const Vec3 &value) {
  return {static_cast<const Quat::Base &>(rotation) *
          static_cast<const Vec3::Base &>(value)};
}

inline Mat4 operator*(const Mat4 &lhs, const Mat4 &rhs) {
  return {static_cast<const Mat4::Base &>(lhs) *
          static_cast<const Mat4::Base &>(rhs)};
}

inline Vec4 operator*(const Mat4 &lhs, const Vec4 &rhs) {
  return (static_cast<const Mat4::Base &>(lhs) *
          static_cast<const Vec4::Base &>(rhs));
}

inline Vec3 operator*(const Mat4 &lhs, const Vec3 &rhs) {
  const glm::vec4 transformed =
      static_cast<const Mat4::Base &>(lhs) *
      glm::vec4(static_cast<const Vec3::Base &>(rhs), 1.0f);
  return Vec3(transformed.x, transformed.y, transformed.z);
}

struct AABox {
  Vec3 mMin = Vec3::sReplicate(std::numeric_limits<f32>::max());
  Vec3 mMax = Vec3::sReplicate(-std::numeric_limits<f32>::max());

  void Encapsulate(const Vec3 &value) {
    mMin = Vec3(glm::min(static_cast<const Vec3::Base &>(mMin),
                         static_cast<const Vec3::Base &>(value)));
    mMax = Vec3(glm::max(static_cast<const Vec3::Base &>(mMax),
                         static_cast<const Vec3::Base &>(value)));
  }
};

inline ::Vector2 to_rl(Vec2 value) { return {value.x, value.y}; }

inline Vec2 from_rl(::Vector2 value) { return Vec2(value.x, value.y); }

inline ::Vector3 to_rl(Vec3 value) { return {value.x, value.y, value.z}; }

inline Vec3 from_rl(::Vector3 value) { return Vec3(value.x, value.y, value.z); }

inline ::Vector4 to_rl(Vec4 value) {
  return {value.x, value.y, value.z, value.w};
}

inline Vec4 from_rl(::Vector4 value) {
  return Vec4(value.x, value.y, value.z, value.w);
}

inline ::Vector4 to_rl(Quat value) {
  return {value.x, value.y, value.z, value.w};
}

inline ::Matrix to_rl(Mat4 value) {
  ::Matrix result{};
  result.m0 = value(0, 0);
  result.m4 = value(0, 1);
  result.m8 = value(0, 2);
  result.m12 = value(0, 3);
  result.m1 = value(1, 0);
  result.m5 = value(1, 1);
  result.m9 = value(1, 2);
  result.m13 = value(1, 3);
  result.m2 = value(2, 0);
  result.m6 = value(2, 1);
  result.m10 = value(2, 2);
  result.m14 = value(2, 3);
  result.m3 = value(3, 0);
  result.m7 = value(3, 1);
  result.m11 = value(3, 2);
  result.m15 = value(3, 3);
  return result;
}

inline Mat4 from_rl(const ::Matrix &value) {
  return Mat4(Vec4(value.m0, value.m1, value.m2, value.m3),
              Vec4(value.m4, value.m5, value.m6, value.m7),
              Vec4(value.m8, value.m9, value.m10, value.m11),
              Vec4(value.m12, value.m13, value.m14, value.m15));
}

} // namespace rydz_math
