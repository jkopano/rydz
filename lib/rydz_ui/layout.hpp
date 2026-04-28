#pragma once
#include "math.hpp"
#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_ui/components.hpp"
#include <algorithm>

namespace rydz::ui {

namespace helpers {
// placeholder for text measure, before adding rydz_text
// TODO: replace with real font metrics
inline math::Vec2 measure_text(std::string const& text, float fontSize) {
  float width = static_cast<float>(text.length()) * fontSize * 0.6f;
  float height = fontSize;

  return {width, height};
}

// return the primary axis
inline float main_axis(math::Vec2 v, Direction direction) {
  if (Direction::Row == direction) {
    return v.x;
  }
  if (Direction::Column == direction) {
    return v.y;
  }
  return 0.0;
}

// return the cross axis
inline float cross_axis(math::Vec2 v, Direction direction) {
  if (Direction::Row == direction) {
    return v.y;
  }
  if (Direction::Column == direction) {
    return v.x;
  }
  return 0.0;
}

inline math::Vec2 make_vec2(
  float main_axis, float cross_axis, Direction direction
) {
  if (Direction::Row == direction) {
    return {main_axis, cross_axis};
  }
  if (Direction::Column == direction) {
    return {cross_axis, main_axis};
  }
  return {0.0, 0.0};
}

// default Style is defined in struct values
inline Style const& default_style() {
  static Style const s{};
  return s;
}

inline Style const& get_style_or_default(
  ecs::World const& world, ecs::Entity e
) {
  if (auto* s = world.get_component<Style>(e)) {
    return *s;
  }
  return default_style();
}

inline float main_padding(Style const& style, Direction direction) {
  if (Direction::Row == direction) {
    return style.padding.left + style.padding.right;
  }
  if (Direction::Column == direction) {
    return style.padding.bottom + style.padding.top;
  }
  return 0.0f;
}

inline float cross_padding(Style const& style, Direction direction) {
  if (Direction::Row == direction) {
    return style.padding.bottom + style.padding.top;
  }
  if (Direction::Column == direction) {
    return style.padding.left + style.padding.right;
  }
  return 0.0f;
}

inline float main_margin(Style const& style, Direction direction) {
  if (Direction::Row == direction) {
    return style.margin.left + style.margin.right;
  }
  if (Direction::Column == direction) {
    return style.margin.bottom + style.margin.top;
  }
  return 0.0f;
}

inline float cross_margin(Style const& style, Direction direction) {
  if (Direction::Row == direction) {
    return style.margin.bottom + style.margin.top;
  }
  if (Direction::Column == direction) {
    return style.margin.left + style.margin.right;
  }
  return 0.0f;
}

inline float resolve_main_size(
  Style const& style, math::Vec2 content_size, Direction direction
) {
  float main = 0.0;

  if (Direction::Row == direction) {
    main = (style.size.width.kind == SizeKind::Px) ? style.size.width.value
                                                   : content_size.x;
  } else if (Direction::Column == direction) {
    main = (style.size.height.kind == SizeKind::Px) ? style.size.height.value
                                                    : content_size.y;
  }
  return main;
}

inline float resolve_cross_size(
  Style const& style, math::Vec2 content_size, Direction direction
) {
  float cross = 0.0;

  if (Direction::Row == direction) {
    cross = (style.size.height.kind == SizeKind::Px) ? style.size.height.value
                                                     : content_size.y;
  } else if (Direction::Column == direction) {
    cross = (style.size.width.kind == SizeKind::Px) ? style.size.width.value
                                                    : content_size.x;
  }
  return cross;
}

} // namespace helpers

namespace algorithms {
inline void measure_node(ecs::World& world, ecs::Entity entity) {
  Style const& style = helpers::get_style_or_default(world, entity);

  auto* c = world.get_component<ComputedUiNode>(entity);
  // hide from display
  if (nullptr != c) {
    if (style.display == Display::None) {
      c->content_size = {0.0, 0.0};
      return;
    }
  } else {
    world.insert_component(entity, ComputedUiNode{});
    c = world.get_component<ComputedUiNode>(entity);
  }

  // now label can't have child so, it need to be the last one
  // so this is the reason for this return
  auto* l = world.get_component<Label>(entity);
  if (nullptr != l) {
    auto* text_cache = world.get_resource<UiTextCache>();
    auto* textures = world.get_resource<ecs::Assets<ecs::Texture>>();

    if (text_cache && textures) {
      std::string const key = make_label_cache_key(*l);
      auto it = text_cache->items.find(key);
      if (it == text_cache->items.end()) {
        rl::Image img = rl::ImageText(
          l->text.c_str(), static_cast<int>(l->font_size), l->color
        );
        gl::Texture tex = rl::LoadTextureFromImage(img);
        rl::UnloadImage(img);
        auto handle = textures->add(std::move(tex));
        it = text_cache->items.emplace(key, handle).first;
      }

      if (ecs::Texture const* t = textures->get(it->second)) {
        c->content_size = math::Vec2(
          static_cast<float>(t->width), static_cast<float>(t->height)
        );
        return;
      }
    }

    c->content_size = helpers::measure_text(l->text, l->font_size);
    return;
  }

  auto children = ecs::children_of(world, entity);
  float total_main = 0.0;
  float max_cross = 0.0;
  int count = 0;

  for (auto child : children) {
    measure_node(world, child);

    float child_main = 0.0f;
    float child_cross = 0.0f;

    Style const& child_style = helpers::get_style_or_default(world, child);
    if (Display::None == child_style.display) {
      continue;
    }

    auto* c_child = world.get_component<ComputedUiNode>(child);
    if (nullptr == c_child) {
      continue;
    }

    child_main = helpers::main_axis(c_child->content_size, style.direction);
    child_cross = helpers::cross_axis(c_child->content_size, style.direction);

    child_main += helpers::main_margin(child_style, style.direction);
    child_cross += helpers::cross_margin(child_style, style.direction);

    total_main += child_main;
    max_cross = std::max(max_cross, child_cross);

    count++;
  }

  if (count > 1) {
    total_main += style.gap * (static_cast<float>(count) - 1);
  }

  float content_main =
    total_main + helpers::main_padding(style, style.direction);
  float content_cross =
    max_cross + helpers::cross_padding(style, style.direction);

  c->content_size =
    helpers::make_vec2(content_main, content_cross, style.direction);
}

inline void layout_node(
  ecs::World& world, ecs::Entity entity, math::Vec2 position, math::Vec2 size
) {
  Style const& style = helpers::get_style_or_default(world, entity);

  auto get_main_margins = [&](Style const& s) {
    if (style.direction == Direction::Row) {
      return std::pair<float, float>{s.margin.left, s.margin.right};
    }
    return std::pair<float, float>{s.margin.top, s.margin.bottom};
  };

  auto get_cross_margin = [&](Style const& s) {
    return (style.direction == Direction::Row) ? s.margin.top : s.margin.left;
  };

  auto* c = world.get_component<ComputedUiNode>(entity);
  // hide from display
  if (nullptr != c) {
    if (style.display == Display::None) {
      c->pos = position;
      c->size = {0.0, 0.0};
      return;
    }
  } else {
    world.insert_component(entity, ComputedUiNode{});
    c = world.get_component<ComputedUiNode>(entity);
  }
  c->pos = position;
  c->size = size;

  auto children = ecs::children_of(world, entity);
  if (children.empty()) {
    return;
  }

  math::Vec2 inner_position{};
  inner_position.x = position.x + style.padding.left;
  inner_position.y = position.y + style.padding.top;

  math::Vec2 inner_size{};
  inner_size.x = size.x - (style.padding.right + style.padding.left);
  inner_size.y = size.y - (style.padding.top + style.padding.bottom);

  float total_main = 0.0f;
  int count = 0;

  for (auto child : children) {
    Style const& child_style = helpers::get_style_or_default(world, child);
    float child_main = 0.0f;

    if (Display::None == child_style.display) {
      continue;
    }

    auto* c_child = world.get_component<ComputedUiNode>(child);
    if (nullptr == c_child) {
      continue;
    }

    auto [margin_before, margin_after] = get_main_margins(child_style);

    child_main = helpers::resolve_main_size(
      child_style, c_child->content_size, style.direction
    );
    child_main += margin_before + margin_after;

    total_main += child_main;
    count++;
  }

  if (count > 1) {
    total_main += style.gap * (static_cast<float>(count) - 1);
  }

  float free_space =
    helpers::main_axis(inner_size, style.direction) - total_main;
  float offset = 0.0f;
  if (Justify::Center == style.justify) {
    offset = free_space * 0.5f;
  }
  if (Justify::End == style.justify) {
    offset = free_space;
  }

  float cursor = helpers::main_axis(inner_position, style.direction) + offset;

  float child_main = 0.0f;
  float child_cross = 0.0f;
  math::Vec2 child_pos = {0.0f, 0.0f};
  math::Vec2 child_size = {0.0f, 0.0f};
  float cross_pos = 0.0f;

  for (auto child : children) {
    Style const& child_style = helpers::get_style_or_default(world, child);
    if (Display::None == child_style.display) {
      continue;
    }

    auto* c_child = world.get_component<ComputedUiNode>(child);
    if (nullptr == c_child) {
      continue;
    }

    child_main = helpers::resolve_main_size(
      child_style, c_child->content_size, style.direction
    );
    child_cross = helpers::resolve_cross_size(
      child_style, c_child->content_size, style.direction
    );
    auto [margin_before, margin_after] = get_main_margins(child_style);
    float margin_cross = get_cross_margin(child_style);

    cursor += margin_before;

    cross_pos =
      helpers::cross_axis(inner_position, style.direction) + margin_cross;

    if (Align::Center == style.align) {
      cross_pos +=
        (helpers::cross_axis(inner_size, style.direction) - child_cross) * 0.5f;
    }
    if (Align::End == style.align) {
      cross_pos +=
        (helpers::cross_axis(inner_size, style.direction) - child_cross);
    }

    child_pos = helpers::make_vec2(cursor, cross_pos, style.direction);
    child_size = helpers::make_vec2(child_main, child_cross, style.direction);

    layout_node(world, child, child_pos, child_size);

    cursor += child_main;
    cursor += margin_after;
    cursor += style.gap;
  }
}

// main alghorithm to compute layout
inline void compute_layout(
  ecs::World& world, ecs::Entity root, math::Vec2 root_size
) {
  measure_node(world, root);
  layout_node(world, root, {0.0f, 0.0f}, root_size);
}

} // namespace algorithms

} // namespace rydz::ui
