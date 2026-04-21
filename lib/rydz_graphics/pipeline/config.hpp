#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/state.hpp"
#include <algorithm>

namespace ecs {

struct DebugOverlaySettings {
  using T = Resource;

  bool draw_fps = true;
  Vec2 fps_position = {10.0f, 10.0f};
};

struct RenderConfig {
  gl::Depth depth{};
  gl::Blend blend = gl::Blend::alpha();
  gl::Cull cull = gl::Cull::back();
  gl::Polygon polygon{};
  gl::ColorMask color_mask{};

  static auto get_default() -> RenderConfig { return {}; }

  static auto depth_prepass() -> RenderConfig {
    return {
      .depth = {.test = true, .write = true},
      .blend = gl::Blend::disabled(),
      .cull = gl::Cull::back(),
      .polygon = gl::Polygon::fill(),
      .color_mask = {.r = false, .g = false, .b = false, .a = false},
    };
  }

  static auto post_depth_prepass() -> RenderConfig { return get_default(); }

  static auto opaque() -> RenderConfig {
    return {
      .depth = {.test = true, .write = true},
      .blend = gl::Blend::disabled(),
      .cull = gl::Cull::back(),
      .polygon = gl::Polygon::fill(),
      .color_mask = {},
    };
  }

  static auto transparent() -> RenderConfig {
    return {
      .depth = {.test = true, .write = false},
      .blend = gl::Blend::alpha(),
      .cull = gl::Cull::back(),
      .polygon = gl::Polygon::fill(),
      .color_mask = {},
    };
  }
};

} // namespace ecs

namespace gl {

inline auto RenderState::apply(ecs::RenderConfig const& config) -> void {
  bool const color_mask_changed = color_mask_ != config.color_mask;
  bool const depth_changed = depth_ != config.depth;
  bool const blend_changed = blend_ != config.blend;
  bool const cull_changed = cull_ != config.cull;
  bool const polygon_changed = polygon_ != config.polygon;

  if (!color_mask_changed && !depth_changed && !blend_changed &&
      !cull_changed && !polygon_changed) {
    return;
  }

  flush_batch();

  if (color_mask_changed) {
    config.color_mask.apply();
    color_mask_ = config.color_mask;
  }
  if (depth_changed) {
    config.depth.apply();
    depth_ = config.depth;
  }
  if (blend_changed) {
    config.blend.apply();
    blend_ = config.blend;
  }
  if (cull_changed) {
    config.cull.apply();
    cull_ = config.cull;
  }
  if (polygon_changed) {
    config.polygon.apply();
    polygon_ = config.polygon;
  }
}

} // namespace gl
