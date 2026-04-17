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

  Vec2() : Base(0.0F) {}
  Vec2(const Base &value) : Base(value) {}

  static const Vec2 ZERO;
  static auto splat(f32 value) -> Vec2 { return Vec2{value}; }

  [[nodiscard]] auto length_sq() const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto length() const -> f32 {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto normalized() const -> Vec2 {
    const f32 len = length();
    if (len <= 1e-8f) {
      return ZERO;
    }
    return {glm::normalize(static_cast<const Base &>(*this))};
  }

  [[nodiscard]] auto dot(const Vec2 &other) const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(other));
  }
};

inline constexpr Vec2 Vec2::ZERO{0.0f, 0.0f};

struct Vec3 : public glm::vec3 {
  using Base = glm::vec3;
  using Base::Base;

  Vec3() : Base(0.0F) {}
  Vec3(const Base &value) : Base(value) {}

  static const Vec3 ZERO;
  static auto splat(f32 value) -> Vec3 { return Vec3{value}; }

  [[nodiscard]] auto length_sq() const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto length() const -> f32 {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto normalized() const -> Vec3 {
    const f32 len = length();
    if (len <= 1e-8F) {
      return ZERO;
    }
    return {glm::normalize(static_cast<const Base &>(*this))};
  }

  [[nodiscard]] auto dot(const Vec3 &other) const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(other));
  }

  [[nodiscard]] auto cross(const Vec3 &other) const -> Vec3 {
    return {glm::cross(static_cast<const Base &>(*this),
                       static_cast<const Base &>(other))};
  }
};

inline constexpr Vec3 Vec3::ZERO{0.0F, 0.0F, 0.0F};

struct Vec4 : public glm::vec4 {
  using Base = glm::vec4;
  using Base::Base;

  Vec4() : Base(0.0F) {}
  Vec4(const Base &value) : Base(value) {}

  static const Vec4 ZERO;
  static auto splat(f32 value) -> Vec4 { return Vec4{value}; }

  [[nodiscard]] auto length_sq() const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto length() const -> f32 {
    return glm::length(static_cast<const Base &>(*this));
  }

  [[nodiscard]] auto normalized() const -> Vec4 {
    const f32 len = length();
    if (len <= 1e-8F) {
      return ZERO;
    }
    return {glm::normalize(static_cast<const Base &>(*this))};
  }

  [[nodiscard]] auto dot(const Vec4 &other) const -> f32 {
    return glm::dot(static_cast<const Base &>(*this),
                    static_cast<const Base &>(other));
  }
};

inline constexpr Vec4 Vec4::ZERO{0.0F, 0.0F, 0.0F, 0.0F};

struct Quat : public glm::quat {
  using Base = glm::quat;

  Quat() : Base(1.0F, 0.0F, 0.0F, 0.0F) {}
  Quat(const Base &value) : Base(value) {}
  Quat(f32 x, f32 y, f32 z, f32 w) : Base(w, x, y, z) {}

  static const Quat IDENTITY;

  static auto from_euler(Vec3 angles) -> Quat {
    return {glm::quat(static_cast<const Vec3::Base &>(angles))};
  }

  [[nodiscard]] auto normalized() const -> Quat {
    return {glm::normalize(static_cast<const Base &>(*this))};
  }
};

inline const Quat Quat::IDENTITY{0.0F, 0.0F, 0.0F, 1.0F};

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

  static const Mat4 IDENTITY;

  static auto perspective_rh(f32 fov_y_radians, f32 aspect, f32 near_plane,
                             f32 far_plane) -> Mat4 {
    return {
        glm::perspectiveRH_NO(fov_y_radians, aspect, near_plane, far_plane)};
  }

  static auto orthographic_rh(f32 left, f32 right, f32 bottom, f32 top,
                              f32 near_plane, f32 far_plane) -> Mat4 {
    return {glm::orthoRH_NO(left, right, bottom, top, near_plane, far_plane)};
  }

  static auto look_at_rh(Vec3 position, Vec3 target, Vec3 up) -> Mat4 {
    return {glm::lookAtRH(static_cast<const Vec3::Base &>(position),
                          static_cast<const Vec3::Base &>(target),
                          static_cast<const Vec3::Base &>(up))};
  }

  static auto from_rotation_translation(Quat rotation, Vec3 translation)
      -> Mat4 {
    glm::mat4 matrix =
        glm::mat4_cast(static_cast<const Quat::Base &>(rotation));
    matrix[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0F);
    return {matrix};
  }

  [[nodiscard]] auto pre_scaled(Vec3 scale) const -> Mat4 {
    return {glm::scale(static_cast<const Base &>(*this),
                       static_cast<const Vec3::Base &>(scale))};
  }

  [[nodiscard]] auto inverse() const -> Mat4 {
    return {glm::inverse(static_cast<const Base &>(*this))};
  }

  auto set_translation(Vec3 translation) -> void {
    (*this)[3] = glm::vec4(static_cast<const Vec3::Base &>(translation), 1.0F);
  }

  [[nodiscard]] auto to_quat() const -> Quat {
    return (glm::quat_cast(glm::mat3(static_cast<const Base &>(*this))));
  }

  [[nodiscard]] auto translation() const -> Vec3 {
    const glm::vec4 column = (*this)[3];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] auto x_axis() const -> Vec3 {
    const glm::vec4 column = (*this)[0];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] auto y_axis() const -> Vec3 {
    const glm::vec4 column = (*this)[1];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] auto z_axis() const -> Vec3 {
    const glm::vec4 column = (*this)[2];
    return {column.x, column.y, column.z};
  }

  [[nodiscard]] auto transform_vector3(Vec3 value) const -> Vec3 {
    return {glm::mat3(static_cast<const Base &>(*this)) *
            static_cast<const Vec3::Base &>(value)};
  }

  [[nodiscard]] auto operator()(i32 row, i32 column) const -> f32 {
    return (*this)[column][row];
  }

  [[nodiscard]] auto operator()(i32 row, i32 column) -> f32 & {
    return (*this)[column][row];
  }

  [[nodiscard]] auto decompose() const -> Transform;
};

inline const Mat4 Mat4::IDENTITY{1.0F};

struct GlobalTransform;

struct Transform {
  using Required = ecs::Requires<GlobalTransform>;

  Vec3 translation = Vec3::ZERO;
  Quat rotation = Quat::IDENTITY;
  Vec3 scale = Vec3::splat(1.0F);

  static auto from_xyz(f32 x, f32 y, f32 z) -> Transform {
    return Transform{.translation = {Vec3(x, y, z)},
                     .rotation = Quat::IDENTITY,
                     .scale = Vec3::splat(1.0F)};
  }

  static auto from_translation(Vec3 pos) -> Transform {
    return Transform{.translation = pos,
                     .rotation = Quat::IDENTITY,
                     .scale = Vec3::splat(1.0F)};
  }

  static auto from_rotation(Quat q) -> Transform {
    return Transform{
        .translation = Vec3::ZERO, .rotation = q, .scale = Vec3::splat(1.0F)};
  }

  static auto from_scale(Vec3 s) -> Transform {
    return Transform{
        .translation = Vec3::ZERO, .rotation = Quat::IDENTITY, .scale = s};
  }

  [[nodiscard]] auto compute_matrix() const -> Mat4 {
    return Mat4::from_rotation_translation(rotation, translation)
        .pre_scaled(scale);
  }

  [[nodiscard]] auto to_matrix() const -> Mat4 { return compute_matrix(); }

  [[nodiscard]] auto forward() const -> Vec3 {
    return rotation * Vec3(0, 0, -1);
  }

  [[nodiscard]] auto right() const -> Vec3 { return rotation * Vec3(1, 0, 0); }

  [[nodiscard]] auto up() const -> Vec3 { return rotation * Vec3(0, 1, 0); }

  auto look_at(Vec3 target, Vec3 world_up = Vec3(0, 1, 0)) -> Transform & {
    Vec3 dir = target - translation;
    if (dir.length_sq() < 1e-10F) {
      return *this;
    }

    Mat4 look = Mat4::look_at_rh(translation, target, world_up);
    Mat4 inv = look.inverse();
    inv.set_translation(Vec3::ZERO);
    rotation = inv.to_quat().normalized();
    return *this;
  }
};

struct GlobalTransform {
  Mat4 matrix = Mat4::IDENTITY;

  static auto from_matrix(Mat4 m) -> GlobalTransform { return {m}; }

  [[nodiscard]] auto translation() const -> Vec3 {
    return matrix.translation();
  }
  [[nodiscard]] auto forward() const -> Vec3 {
    return Vec3(-matrix.z_axis()).normalized();
  }

  [[nodiscard]] auto right() const -> Vec3 {
    return matrix.x_axis().normalized();
  }
  [[nodiscard]] auto up() const -> Vec3 { return matrix.y_axis().normalized(); }
};

inline auto Mat4::decompose() const -> Transform {
  glm::vec3 scale{};
  glm::quat rotation{};
  glm::vec3 translation{};
  glm::vec3 skew{};
  glm::vec4 perspective{};

  glm::decompose(static_cast<const Base &>(*this), scale, rotation, translation,
                 skew, perspective);

  return Transform{
      .translation = Vec3(translation),
      .rotation = Quat(rotation).normalized(),
      .scale = Vec3(scale),
  };
}

inline auto operator+(Vec2 lhs, const Vec2 &rhs) -> Vec2 {
  return {static_cast<const Vec2::Base &>(lhs) +
          static_cast<const Vec2::Base &>(rhs)};
}

inline auto operator-(Vec2 lhs, const Vec2 &rhs) -> Vec2 {
  return {static_cast<const Vec2::Base &>(lhs) -
          static_cast<const Vec2::Base &>(rhs)};
}

inline auto operator-(const Vec2 &value) -> Vec2 {
  return {-static_cast<const Vec2::Base &>(value)};
}

inline auto operator*(Vec2 lhs, f32 rhs) -> Vec2 {
  return {static_cast<const Vec2::Base &>(lhs) * rhs};
}

inline auto operator*(f32 lhs, Vec2 rhs) -> Vec2 {
  return {lhs * static_cast<const Vec2::Base &>(rhs)};
}

inline auto operator/(Vec2 lhs, f32 rhs) -> Vec2 {
  return {static_cast<const Vec2::Base &>(lhs) / rhs};
}

inline auto operator+(Vec3 lhs, const Vec3 &rhs) -> Vec3 {
  return {static_cast<const Vec3::Base &>(lhs) +
          static_cast<const Vec3::Base &>(rhs)};
}

inline auto operator-(Vec3 lhs, const Vec3 &rhs) -> Vec3 {
  return {static_cast<const Vec3::Base &>(lhs) -
          static_cast<const Vec3::Base &>(rhs)};
}

inline auto operator-(const Vec3 &value) -> Vec3 {
  return {-static_cast<const Vec3::Base &>(value)};
}

inline auto operator*(Vec3 lhs, f32 rhs) -> Vec3 {
  return {static_cast<const Vec3::Base &>(lhs) * rhs};
}

inline auto operator*(f32 lhs, Vec3 rhs) -> Vec3 {
  return {lhs * static_cast<const Vec3::Base &>(rhs)};
}

inline auto operator/(Vec3 lhs, f32 rhs) -> Vec3 {
  return {static_cast<const Vec3::Base &>(lhs) / rhs};
}

inline auto operator+(Vec4 lhs, const Vec4 &rhs) -> Vec4 {
  return {static_cast<const Vec4::Base &>(lhs) +
          static_cast<const Vec4::Base &>(rhs)};
}

inline auto operator-(Vec4 lhs, const Vec4 &rhs) -> Vec4 {
  return {static_cast<const Vec4::Base &>(lhs) -
          static_cast<const Vec4::Base &>(rhs)};
}

inline auto operator-(const Vec4 &value) -> Vec4 {
  return {-static_cast<const Vec4::Base &>(value)};
}

inline auto operator*(Vec4 lhs, f32 rhs) -> Vec4 {
  return {static_cast<const Vec4::Base &>(lhs) * rhs};
}

inline auto operator*(f32 lhs, Vec4 rhs) -> Vec4 {
  return {lhs * static_cast<const Vec4::Base &>(rhs)};
}

inline auto operator/(Vec4 lhs, f32 rhs) -> Vec4 {
  return {static_cast<const Vec4::Base &>(lhs) / rhs};
}

inline auto operator*(const Quat &lhs, const Quat &rhs) -> Quat {
  return {static_cast<const Quat::Base &>(lhs) *
          static_cast<const Quat::Base &>(rhs)};
}

inline auto operator*(const Quat &rotation, const Vec3 &value) -> Vec3 {
  return {static_cast<const Quat::Base &>(rotation) *
          static_cast<const Vec3::Base &>(value)};
}

inline auto operator*(const Mat4 &lhs, const Mat4 &rhs) -> Mat4 {
  return {static_cast<const Mat4::Base &>(lhs) *
          static_cast<const Mat4::Base &>(rhs)};
}

inline auto operator*(const Mat4 &lhs, const Vec4 &rhs) -> Vec4 {
  return (static_cast<const Mat4::Base &>(lhs) *
          static_cast<const Vec4::Base &>(rhs));
}

inline auto operator*(const Mat4 &lhs, const Vec3 &rhs) -> Vec3 {
  const glm::vec4 transformed =
      static_cast<const Mat4::Base &>(lhs) *
      glm::vec4(static_cast<const Vec3::Base &>(rhs), 1.0f);
  return Vec3(transformed.x, transformed.y, transformed.z);
}

struct AABox {
  Vec3 mMin = Vec3::splat(std::numeric_limits<f32>::max());
  Vec3 mMax = Vec3::splat(-std::numeric_limits<f32>::max());

  auto encapsulate(const Vec3 &value) -> void {
    mMin = Vec3(glm::min(static_cast<const Vec3::Base &>(mMin),
                         static_cast<const Vec3::Base &>(value)));
    mMax = Vec3(glm::max(static_cast<const Vec3::Base &>(mMax),
                         static_cast<const Vec3::Base &>(value)));
  }
};

inline auto to_rl(Vec2 value) -> ::Vector2 { return {value.x, value.y}; }

inline auto from_rl(::Vector2 value) -> Vec2 { return Vec2(value.x, value.y); }

inline auto to_rl(Vec3 value) -> ::Vector3 {
  return {value.x, value.y, value.z};
}

inline auto from_rl(::Vector3 value) -> Vec3 {
  return Vec3(value.x, value.y, value.z);
}

inline auto to_rl(Vec4 value) -> ::Vector4 {
  return {value.x, value.y, value.z, value.w};
}

inline auto from_rl(::Vector4 value) -> Vec4 {
  return Vec4(value.x, value.y, value.z, value.w);
}

inline auto to_rl(Quat value) -> ::Vector4 {
  return {value.x, value.y, value.z, value.w};
}

inline auto to_rl(Mat4 value) -> ::Matrix {
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

inline auto from_rl(const ::Matrix &value) -> Mat4 {
  return Mat4(Vec4(value.m0, value.m1, value.m2, value.m3),
              Vec4(value.m4, value.m5, value.m6, value.m7),
              Vec4(value.m8, value.m9, value.m10, value.m11),
              Vec4(value.m12, value.m13, value.m14, value.m15));
}

} // namespace rydz_math
