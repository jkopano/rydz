#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/mod.hpp"
#include <string>
#include <unordered_map>

namespace rydz::ui {

struct UiRoot {
  using T = ecs::Resource;
  ecs::Entity root;
};

// struct UiWhiteTexture {
//   using T = ecs::Resource;
//   ecs::Handle<ecs::Material> material;
// };

struct UiTextCache {
  using T = ecs::Resource;
  std::unordered_map<std::string, ecs::Handle<ecs::Texture>> items;
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

enum struct Direction { Row, Column };
enum struct Align { Start, Center, End };
enum struct Justify { Start, Center, End };
enum struct SizeKind { Auto, Px };

struct SizeValue {
  SizeKind kind = SizeKind::Auto;
  float value = 0.0f;

  static SizeValue auto_() { return {SizeKind::Auto, 0.0}; }
  static SizeValue px(float val) { return {SizeKind::Px, val}; }
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
  Size2 size = {{SizeKind::Auto, 0.0}, {SizeKind::Auto, 0.0}};
};

struct Panel {
  rl::Color background_color = rl::Color{WHITE};
};

struct Label {
  std::string text = "";
  float font_size = 16.0;
  rl::Color color = rl::Color{BLACK};
};

inline std::string make_label_cache_key(const Label &label) {
  return label.text + "|" + std::to_string(label.font_size) + "|" +
         std::to_string(label.color.r) + "," + std::to_string(label.color.g) +
         "," + std::to_string(label.color.b) + "," +
         std::to_string(label.color.a);
}

} // namespace rydz::ui
