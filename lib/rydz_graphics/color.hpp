#pragma once

#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/gl/core.hpp"

namespace ecs {

struct Color {
  u8 r = 255;
  u8 g = 255;
  u8 b = 255;
  u8 a = 255;

  constexpr Color() = default;
  constexpr Color(u8 red, u8 green, u8 blue, u8 alpha = 255)
      : r(red), g(green), b(blue), a(alpha) {}
  constexpr Color(rydz_gl::Color raw)
      : r(raw.r), g(raw.g), b(raw.b), a(raw.a) {}

  constexpr operator rydz_gl::Color() const { return {r, g, b, a}; }

  bool operator==(const Color &) const = default;
};

inline constexpr Color kWhite = {255, 255, 255, 255};
inline constexpr Color kBlack = {0, 0, 0, 255};
inline constexpr Color kRed = {230, 41, 55, 255};
inline constexpr Color kGreen = {0, 228, 48, 255};
inline constexpr Color kBlue = {0, 121, 241, 255};
inline constexpr Color kYellow = {253, 249, 0, 255};
inline constexpr Color kPurple = {200, 122, 255, 255};
inline constexpr Color kDarkGray = {80, 80, 80, 255};
inline constexpr Color kOrange = {255, 161, 0, 255};

inline rydz_gl::Vec3 color_to_vec3(Color color) {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
}

struct ClearColor {
  using T = Component;
  Color color = {30, 30, 40, 255};
};

} // namespace ecs
