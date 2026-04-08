#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <raylib.h>

namespace rydz_math {

struct Vec2 : public glm::vec2 {
  using Base = glm::vec2;
  using Base::Base;

  Vec2() : Base(0.0f) {}
  Vec2(const Base &value) : Base(value) {}

  static Vec2 sZero() { return Vec2(0.0f); }
  static Vec2 sReplicate(float value) { return Vec2(value); }

  [[nodiscard]] float GetX() const { return x; }
  [[nodiscard]] float GetY() const { return y; }

  [[nodiscard]] float LengthSq() const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] float Length() const {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] Vec2 Normalized() const {
    const float len = Length();
    if (len <= 1e-8f) {
      return sZero();
    }
    return Vec2(glm::normalize(static_cast<const Base &>(*this)));
  }

  [[nodiscard]] float Dot(const Vec2 &other) const {
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
  static Vec3 sReplicate(float value) { return Vec3(value); }

  [[nodiscard]] float GetX() const { return x; }
  [[nodiscard]] float GetY() const { return y; }
  [[nodiscard]] float GetZ() const { return z; }

  [[nodiscard]] float LengthSq() const {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] float Length() const {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] Vec3 Normalized() const {
    const float len = Length();
    if (len <= 1e-8f) {
      return sZero();
    }
    return Vec3(glm::normalize(static_cast<const Base &>(*this)));
  }

  [[nodiscard]] float Dot(const Vec3 &other) const {
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

  Vec4() : Base(0.0f) {}
  Vec4(const Base &value) : Base(value) {}

  static Vec4 sZero() { return Vec4(0.0f); }
  static Vec4 sReplicate(float value) { return Vec4(value); }

  [[nodiscard]] float GetX() const { return x; }
  [[nodiscard]] float GetY() const { return y; }
  [[nodiscard]] float GetZ() const { return z; }
  [[nodiscard]] float GetW() const { return w; }
};

struct Quat : public glm::quat {
  using Base = glm::quat;

  Quat() : Base(1.0f, 0.0f, 0.0f, 0.0f) {}
  Quat(const Base &value) : Base(value) {}
  Quat(float x, float y, float z, float w) : Base(w, x, y, z) {}

  static Quat sIdentity() { return Quat(0.0f, 0.0f, 0.0f, 1.0f); }

  static Quat sEulerAngles(const Vec3 &angles) {
    return Quat(glm::quat(static_cast<const Vec3::Base &>(angles)));
  }

  [[nodiscard]] float GetX() const { return x; }
  [[nodiscard]] float GetY() const { return y; }
  [[nodiscard]] float GetZ() const { return z; }
  [[nodiscard]] float GetW() const { return w; }

  [[nodiscard]] Quat Normalized() const {
    return Quat(glm::normalize(static_cast<const Base &>(*this)));
  }
};

struct Mat4 : public glm::mat4 {
public:
  using Base = glm::mat4;

  Mat4() : Base(1.0f) {}
  Mat4(const Base &value) : Base(value) {}
  explicit Mat4(float diagonal) : Base(diagonal) {}
  Mat4(const Vec4 &column0, const Vec4 &column1, const Vec4 &column2,
       const Vec4 &column3)
      : Base(static_cast<const Vec4::Base &>(column0),
             static_cast<const Vec4::Base &>(column1),
             static_cast<const Vec4::Base &>(column2),
             static_cast<const Vec4::Base &>(column3)) {}

  static Mat4 sIdentity() { return Mat4(1.0f); }

  static Mat4 sPerspective(float fov_y_radians, float aspect, float near_plane,
                           float far_plane) {
    return Mat4(
        glm::perspectiveRH_NO(fov_y_radians, aspect, near_plane, far_plane));
  }

  static Mat4 sOrthographic(float left, float right, float bottom, float top,
                            float near_plane, float far_plane) {
    return Mat4(
        glm::orthoRH_NO(left, right, bottom, top, near_plane, far_plane));
  }

  static Mat4 sLookAt(const Vec3 &position, const Vec3 &target,
                      const Vec3 &up) {
    return Mat4(glm::lookAtRH(static_cast<const Vec3::Base &>(position),
                              static_cast<const Vec3::Base &>(target),
                              static_cast<const Vec3::Base &>(up)));
  }

  static Mat4 sRotationTranslation(const Quat &rotation,
                                   const Vec3 &translation) {
    glm::mat4 matrix =
        glm::mat4_cast(static_cast<const Quat::Base &>(rotation));
    matrix[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0f);
    return Mat4(matrix);
  }

  [[nodiscard]] Mat4 PreScaled(const Vec3 &scale) const {
    return Mat4(glm::scale(static_cast<const Base &>(*this),
                           static_cast<const Vec3::Base &>(scale)));
  }

  [[nodiscard]] Mat4 Inversed() const {
    return Mat4(glm::inverse(static_cast<const Base &>(*this)));
  }

  void SetTranslation(const Vec3 &translation) {
    (*this)[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0f);
  }

  [[nodiscard]] Quat GetQuaternion() const {
    return Quat(glm::quat_cast(glm::mat3(static_cast<const Base &>(*this))));
  }

  [[nodiscard]] Vec3 GetTranslation() const {
    const glm::vec4 column = (*this)[3];
    return Vec3(column.x, column.y, column.z);
  }

  [[nodiscard]] Vec3 GetAxisX() const {
    const glm::vec4 column = (*this)[0];
    return Vec3(column.x, column.y, column.z);
  }

  [[nodiscard]] Vec3 GetAxisY() const {
    const glm::vec4 column = (*this)[1];
    return Vec3(column.x, column.y, column.z);
  }

  [[nodiscard]] Vec3 GetAxisZ() const {
    const glm::vec4 column = (*this)[2];
    return Vec3(column.x, column.y, column.z);
  }

  [[nodiscard]] Vec3 Multiply3x3(const Vec3 &value) const {
    return Vec3(glm::mat3(static_cast<const Base &>(*this)) *
                static_cast<const Vec3::Base &>(value));
  }

  [[nodiscard]] float operator()(int row, int column) const {
    return (*this)[column][row];
  }

  [[nodiscard]] float &operator()(int row, int column) {
    return (*this)[column][row];
  }
};

inline Vec2 operator+(Vec2 lhs, const Vec2 &rhs) {
  return Vec2(static_cast<const Vec2::Base &>(lhs) +
              static_cast<const Vec2::Base &>(rhs));
}

inline Vec2 operator-(Vec2 lhs, const Vec2 &rhs) {
  return Vec2(static_cast<const Vec2::Base &>(lhs) -
              static_cast<const Vec2::Base &>(rhs));
}

inline Vec2 operator-(const Vec2 &value) {
  return Vec2(-static_cast<const Vec2::Base &>(value));
}

inline Vec2 operator*(Vec2 lhs, float rhs) {
  return Vec2(static_cast<const Vec2::Base &>(lhs) * rhs);
}

inline Vec2 operator*(float lhs, Vec2 rhs) {
  return Vec2(lhs * static_cast<const Vec2::Base &>(rhs));
}

inline Vec2 operator/(Vec2 lhs, float rhs) {
  return Vec2(static_cast<const Vec2::Base &>(lhs) / rhs);
}

inline Vec3 operator+(Vec3 lhs, const Vec3 &rhs) {
  return Vec3(static_cast<const Vec3::Base &>(lhs) +
              static_cast<const Vec3::Base &>(rhs));
}

inline Vec3 operator-(Vec3 lhs, const Vec3 &rhs) {
  return Vec3(static_cast<const Vec3::Base &>(lhs) -
              static_cast<const Vec3::Base &>(rhs));
}

inline Vec3 operator-(const Vec3 &value) {
  return Vec3(-static_cast<const Vec3::Base &>(value));
}

inline Vec3 operator*(Vec3 lhs, float rhs) {
  return Vec3(static_cast<const Vec3::Base &>(lhs) * rhs);
}

inline Vec3 operator*(float lhs, Vec3 rhs) {
  return Vec3(lhs * static_cast<const Vec3::Base &>(rhs));
}

inline Vec3 operator/(Vec3 lhs, float rhs) {
  return Vec3(static_cast<const Vec3::Base &>(lhs) / rhs);
}

inline Vec4 operator+(Vec4 lhs, const Vec4 &rhs) {
  return Vec4(static_cast<const Vec4::Base &>(lhs) +
              static_cast<const Vec4::Base &>(rhs));
}

inline Vec4 operator-(Vec4 lhs, const Vec4 &rhs) {
  return Vec4(static_cast<const Vec4::Base &>(lhs) -
              static_cast<const Vec4::Base &>(rhs));
}

inline Vec4 operator-(const Vec4 &value) {
  return Vec4(-static_cast<const Vec4::Base &>(value));
}

inline Vec4 operator*(Vec4 lhs, float rhs) {
  return Vec4(static_cast<const Vec4::Base &>(lhs) * rhs);
}

inline Vec4 operator*(float lhs, Vec4 rhs) {
  return Vec4(lhs * static_cast<const Vec4::Base &>(rhs));
}

inline Vec4 operator/(Vec4 lhs, float rhs) {
  return Vec4(static_cast<const Vec4::Base &>(lhs) / rhs);
}

inline Quat operator*(const Quat &lhs, const Quat &rhs) {
  return Quat(static_cast<const Quat::Base &>(lhs) *
              static_cast<const Quat::Base &>(rhs));
}

inline Vec3 operator*(const Quat &rotation, const Vec3 &value) {
  return Vec3(static_cast<const Quat::Base &>(rotation) *
              static_cast<const Vec3::Base &>(value));
}

inline Mat4 operator*(const Mat4 &lhs, const Mat4 &rhs) {
  return Mat4(static_cast<const Mat4::Base &>(lhs) *
              static_cast<const Mat4::Base &>(rhs));
}

inline Vec4 operator*(const Mat4 &lhs, const Vec4 &rhs) {
  return Vec4(static_cast<const Mat4::Base &>(lhs) *
              static_cast<const Vec4::Base &>(rhs));
}

inline Vec3 operator*(const Mat4 &lhs, const Vec3 &rhs) {
  const glm::vec4 transformed =
      static_cast<const Mat4::Base &>(lhs) *
      glm::vec4(static_cast<const Vec3::Base &>(rhs), 1.0f);
  return Vec3(transformed.x, transformed.y, transformed.z);
}

struct AABox {
  Vec3 mMin = Vec3::sReplicate(std::numeric_limits<float>::max());
  Vec3 mMax = Vec3::sReplicate(-std::numeric_limits<float>::max());

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
