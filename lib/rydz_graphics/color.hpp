#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "types.hpp"

#ifdef LIGHTGRAY
#undef LIGHTGRAY
#endif
#ifdef GRAY
#undef GRAY
#endif
#ifdef DARKGRAY
#undef DARKGRAY
#endif
#ifdef YELLOW
#undef YELLOW
#endif
#ifdef GOLD
#undef GOLD
#endif
#ifdef ORANGE
#undef ORANGE
#endif
#ifdef PINK
#undef PINK
#endif
#ifdef RED
#undef RED
#endif
#ifdef MAROON
#undef MAROON
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef LIME
#undef LIME
#endif
#ifdef DARKGREEN
#undef DARKGREEN
#endif
#ifdef SKYBLUE
#undef SKYBLUE
#endif
#ifdef BLUE
#undef BLUE
#endif
#ifdef DARKBLUE
#undef DARKBLUE
#endif
#ifdef PURPLE
#undef PURPLE
#endif
#ifdef VIOLET
#undef VIOLET
#endif
#ifdef DARKPURPLE
#undef DARKPURPLE
#endif
#ifdef BEIGE
#undef BEIGE
#endif
#ifdef BROWN
#undef BROWN
#endif
#ifdef DARKBROWN
#undef DARKBROWN
#endif
#ifdef WHITE
#undef WHITE
#endif
#ifdef BLACK
#undef BLACK
#endif
#ifdef BLANK
#undef BLANK
#endif
#ifdef MAGENTA
#undef MAGENTA
#endif
#ifdef RAYWHITE
#undef RAYWHITE
#endif

namespace ecs {

struct Color {
  u8 r = 255;
  u8 g = 255;
  u8 b = 255;
  u8 a = 255;

  constexpr Color() = default;
  constexpr Color(u8 red, u8 green, u8 blue, u8 alpha = 255)
      : r(red), g(green), b(blue), a(alpha) {}
  constexpr Color(rl::rlColor raw) : r(raw.r), g(raw.g), b(raw.b), a(raw.a) {}

  constexpr operator rl::rlColor const() const { return {r, g, b, a}; }
  constexpr operator math::Vec3 const() const {
    return {r / 255.f, g / 255.f, b / 255.f};
  }

  auto operator==(Color const&) const -> bool = default;

  static Color const LIGHTGRAY;
  static Color const GRAY;
  static Color const DARKGRAY;
  static Color const YELLOW;
  static Color const GOLD;
  static Color const ORANGE;
  static Color const PINK;
  static Color const RED;
  static Color const MAROON;
  static Color const GREEN;
  static Color const LIME;
  static Color const DARKGREEN;
  static Color const SKYBLUE;
  static Color const BLUE;
  static Color const DARKBLUE;
  static Color const PURPLE;
  static Color const VIOLET;
  static Color const DARKPURPLE;
  static Color const BEIGE;
  static Color const BROWN;
  static Color const DARKBROWN;
  static Color const WHITE;
  static Color const BLACK;
  static Color const BLANK;
  static Color const MAGENTA;
  static Color const RAYWHITE;
};

inline constexpr Color Color::LIGHTGRAY = {200, 200, 200, 255};
inline constexpr Color Color::GRAY = {130, 130, 130, 255};
inline constexpr Color Color::DARKGRAY = {80, 80, 80, 255};
inline constexpr Color Color::YELLOW = {253, 249, 0, 255};
inline constexpr Color Color::GOLD = {255, 203, 0, 255};
inline constexpr Color Color::ORANGE = {255, 161, 0, 255};
inline constexpr Color Color::PINK = {255, 109, 194, 255};
inline constexpr Color Color::RED = {230, 41, 55, 255};
inline constexpr Color Color::MAROON = {190, 33, 55, 255};
inline constexpr Color Color::GREEN = {0, 228, 48, 255};
inline constexpr Color Color::LIME = {0, 158, 47, 255};
inline constexpr Color Color::DARKGREEN = {0, 117, 44, 255};
inline constexpr Color Color::SKYBLUE = {102, 191, 255, 255};
inline constexpr Color Color::BLUE = {0, 121, 241, 255};
inline constexpr Color Color::DARKBLUE = {0, 82, 172, 255};
inline constexpr Color Color::PURPLE = {200, 122, 255, 255};
inline constexpr Color Color::VIOLET = {135, 60, 190, 255};
inline constexpr Color Color::DARKPURPLE = {112, 31, 126, 255};
inline constexpr Color Color::BEIGE = {211, 176, 131, 255};
inline constexpr Color Color::BROWN = {127, 106, 79, 255};
inline constexpr Color Color::DARKBROWN = {76, 63, 47, 255};
inline constexpr Color Color::WHITE = {255, 255, 255, 255};
inline constexpr Color Color::BLACK = {0, 0, 0, 255};
inline constexpr Color Color::BLANK = {0, 0, 0, 0};
inline constexpr Color Color::MAGENTA = {255, 0, 255, 255};
inline constexpr Color Color::RAYWHITE = {245, 245, 245, 255};

struct ClearColor {
  using T = Component;
  Color color = {30, 30, 40, 255};
};

} // namespace ecs
