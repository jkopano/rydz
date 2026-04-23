#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "types.hpp"
#include <algorithm>
#include <bit>
#include <type_traits>

using namespace math;

namespace gl {

using BoneInfo = ::BoneInfo;
using ModelAnimPose = ::ModelAnimPose;
using ModelSkeleton = ::ModelSkeleton;

struct Rectangle {
  f32 x = 0.0F;
  f32 y = 0.0F;
  f32 width = 0.0F;
  f32 height = 0.0F;

  constexpr Rectangle() = default;
  constexpr Rectangle(f32 x, f32 y, f32 width, f32 height) noexcept
      : x(x), y(y), width(width), height(height) {}
  constexpr Rectangle(::rlRectangle const& raw) noexcept
      : x(raw.x), y(raw.y), width(raw.width), height(raw.height) {}

  constexpr operator ::rlRectangle() const noexcept {
    return {.x = x, .y = y, .width = width, .height = height};
  }

  [[nodiscard]] constexpr auto left() const noexcept -> f32 { return x; }
  [[nodiscard]] constexpr auto top() const noexcept -> f32 { return y; }
  [[nodiscard]] constexpr auto right() const noexcept -> f32 { return x + width; }
  [[nodiscard]] constexpr auto bottom() const noexcept -> f32 { return y + height; }
  [[nodiscard]] constexpr auto position() const noexcept -> Vec2 { return {x, y}; }
  [[nodiscard]] constexpr auto size() const noexcept -> Vec2 { return {width, height}; }
  [[nodiscard]] constexpr auto center() const noexcept -> Vec2 {
    return {x + (width * 0.5f), y + (0.5f * height)};
  }
  [[nodiscard]] constexpr auto area() const noexcept -> f32 { return width * height; }
  [[nodiscard]] constexpr auto empty() const noexcept -> bool {
    return width == 0.0F || height == 0.0F;
  }

  [[nodiscard]] constexpr auto normalized() const noexcept -> Rectangle {
    Rectangle result = *this;
    if (result.width < 0.0F) {
      result.x += result.width;
      result.width = -result.width;
    }
    if (result.height < 0.0F) {
      result.y += result.height;
      result.height = -result.height;
    }
    return result;
  }

  [[nodiscard]] constexpr auto contains(Vec2 point) const noexcept -> bool {
    Rectangle const rect = normalized();
    return point.x >= rect.left() && point.x <= rect.right() && point.y >= rect.top() &&
           point.y <= rect.bottom();
  }

  [[nodiscard]] constexpr auto overlaps(Rectangle other) const noexcept -> bool {
    Rectangle const a = normalized();
    Rectangle const b = other.normalized();
    return a.left() < b.right() && a.right() > b.left() && a.top() < b.bottom() &&
           a.bottom() > b.top();
  }

  [[nodiscard]] constexpr auto intersection(Rectangle other) const noexcept -> Rectangle {
    Rectangle const a = normalized();
    Rectangle const b = other.normalized();
    f32 const ix = std::max(a.left(), b.left());
    f32 const iy = std::max(a.top(), b.top());
    f32 const ir = std::min(a.right(), b.right());
    f32 const ib = std::min(a.bottom(), b.bottom());
    if (ir <= ix || ib <= iy) {
      return {ix, iy, 0.0F, 0.0F};
    }
    return {ix, iy, ir - ix, ib - iy};
  }

  [[nodiscard]] constexpr auto translated(Vec2 delta) const noexcept -> Rectangle {
    return {x + delta.x, y + delta.y, width, height};
  }

  [[nodiscard]] constexpr auto resized(Vec2 new_size) const noexcept -> Rectangle {
    return {x, y, new_size.x, new_size.y};
  }

  [[nodiscard]] constexpr auto flipped_y() const noexcept -> Rectangle {
    return {x, y, width, -height};
  }

  [[nodiscard]] constexpr auto flipped_x() const noexcept -> Rectangle {
    return {x, y, -width, height};
  }
};

inline constexpr int SHADER_UNIFORM_FLOAT = RL_SHADER_UNIFORM_FLOAT;
inline constexpr int SHADER_UNIFORM_VEC2 = RL_SHADER_UNIFORM_VEC2;
inline constexpr int SHADER_UNIFORM_VEC3 = RL_SHADER_UNIFORM_VEC3;
inline constexpr int SHADER_UNIFORM_VEC4 = RL_SHADER_UNIFORM_VEC4;
inline constexpr int SHADER_UNIFORM_INT = RL_SHADER_UNIFORM_INT;
inline constexpr int SHADER_UNIFORM_IVEC2 = RL_SHADER_UNIFORM_IVEC2;
inline constexpr int SHADER_UNIFORM_IVEC3 = RL_SHADER_UNIFORM_IVEC3;
inline constexpr int SHADER_UNIFORM_IVEC4 = RL_SHADER_UNIFORM_IVEC4;

inline constexpr int PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 =
  ::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
inline constexpr int TEXTURE_FILTER_BILINEAR = ::TEXTURE_FILTER_BILINEAR;

inline auto default_texture_id() -> unsigned int { return rl::rlGetTextureIdDefault(); }

namespace detail {

template <typename To, typename From>
[[nodiscard]] constexpr auto raylib_cast(From const& value) noexcept -> To {
  static_assert(sizeof(To) == sizeof(From));
  static_assert(alignof(To) == alignof(From));
  static_assert(std::is_trivially_copyable_v<To>);
  static_assert(std::is_trivially_copyable_v<From>);
  return std::bit_cast<To>(value);
}

} // namespace detail

static_assert(sizeof(Rectangle) == sizeof(::rlRectangle));
static_assert(alignof(Rectangle) == alignof(::rlRectangle));
static_assert(std::is_trivially_copyable_v<Rectangle>);

struct Shader {
  u32 id{};
  i32* locs{};

  constexpr Shader() = default;
  constexpr Shader(::Shader const& raw) noexcept
      : Shader(detail::raylib_cast<Shader>(raw)) {}

  constexpr operator ::Shader() const noexcept {
    return detail::raylib_cast<::Shader>(*this);
  }

  auto operator=(::Shader const& raw) noexcept -> Shader& {
    *this = Shader(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id != 0; }
  [[nodiscard]] auto has_locations() const -> bool { return locs != nullptr; }

  auto uniform_location(char const* name) const -> i32 {
    return rl::GetShaderLocation(*this, name);
  }

  auto attribute_location(char const* name) const -> i32 {
    return rl::GetShaderLocationAttrib(*this, name);
  }

  static auto get_default() -> Shader {
    Shader shader = {};
    shader.id = default_id();
    shader.locs = default_locs();
    return shader;
  }
  static auto default_id() -> u32 { return rl::rlGetShaderIdDefault(); }
  static auto default_locs() -> i32* { return rl::rlGetShaderLocsDefault(); }
};

static_assert(sizeof(Shader) == sizeof(::Shader));
static_assert(alignof(Shader) == alignof(::Shader));
static_assert(std::is_trivially_copyable_v<Shader>);

} // namespace gl
