#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/mod.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace rydz::ui {

struct UiRoot {
  using T = ecs::Resource;
  ecs::Entity root;
};

struct UiWhiteTexture {
  using T = ecs::Resource;
  ecs::Handle<ecs::Texture> handle;
};

struct UiTextCache {
  using T = ecs::Resource;
  std::unordered_map<std::string, ecs::Handle<ecs::Texture>> items;
};

struct UiFontCache {
  using T = ecs::Resource;
  std::unordered_map<std::string, rl::Font> items;
};

struct UiQuadMesh {
  using T = ecs::Resource;
  ecs::Handle<ecs::Mesh> mesh;
};

struct UiNode {
  int z_index = 0;
};

struct ComputedUiNode {
  math::Vec2 pos;
  math::Vec2 size;
  math::Vec2 content_size;
};

struct UiPointerState {
  using T = ecs::Resource;

  math::Vec2 cursor{};
  bool primary_down = false;
  bool primary_pressed = false;
  bool primary_released = false;
  int primary_button = 0;

  auto clear_frame() -> void {
    primary_pressed = false;
    primary_released = false;
  }

  auto set_cursor(math::Vec2 value) -> void { cursor = value; }

  auto set_primary_down(int button = 0) -> void {
    if (!primary_down) {
      primary_pressed = true;
    }
    primary_down = true;
    primary_button = button;
  }

  auto set_primary_up(int button = 0) -> void {
    if (primary_down) {
      primary_released = true;
    }
    primary_down = false;
    primary_button = button;
  }
};

struct UiInteractionState {
  using T = ecs::Resource;

  std::optional<ecs::Entity> hovered_entity{};
  std::optional<ecs::Entity> focused_entity{};
  std::optional<ecs::Entity> pressed_entity{};
};

enum struct UiSystemSet { Prepare, Layout, Interaction, PostLayout, Render };

enum struct Direction { Row, Column };
enum struct Align { Start, Center, End };
enum struct Justify { Start, Center, End };
enum struct SizeKind { Auto, Px };

struct SizeValue {
  SizeKind kind = SizeKind::Auto;
  float value = 0.0f;

  static auto auto_() -> SizeValue { return {.kind = SizeKind::Auto, .value = 0.0}; }
  static auto px(float val) -> SizeValue { return {.kind = SizeKind::Px, .value = val}; }
};

struct Size2 {
  SizeValue width;
  SizeValue height;
};

struct UiRect {
  float left = 0, top = 0, right = 0, bottom = 0;
};

// defines the layout model - different layout alghorithms
// Flex - flexbox layout
// grid - grid css layout
// None - hide the node
enum struct Display { Flex, Grid, None };

struct Style {
  Direction direction = Direction::Row;
  Align align = Align::Start;
  Justify justify = Justify::Start;
  UiRect padding{};
  UiRect margin{};
  Display display = Display::Flex;
  float gap = 0.0;
  Size2 size = {.width = {SizeKind::Auto, 0.0}, .height = {SizeKind::Auto, 0.0}};
};

struct Panel {
  ecs::Color background_color = ecs::Color::WHITE;
};

struct Label {
  std::string text;
  float font_size = 16.0;
  rlColor color = rlColor{rlColor{.r = 0, .g = 0, .b = 0, .a = 255}};
  std::string font_path;
  float spacing = 1.0f;
};

inline auto make_label_cache_key(Label const& label) -> std::string {
  return label.text + "|" + std::to_string(label.font_size) + "|" +
         std::to_string(label.color.r) + "," + std::to_string(label.color.g) + "," +
         std::to_string(label.color.b) + "," + std::to_string(label.color.a) + "|" +
         label.font_path + "|" + std::to_string(label.spacing);
}

} // namespace rydz::ui
