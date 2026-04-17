#pragma once

#include "rydz_ecs/params.hpp"
#include "rydz_graphics/gl/state.hpp"

namespace ecs {

struct ColorMask {
  bool r = true;
  bool g = true;
  bool b = true;
  bool a = true;
};

struct RenderConfig {
  gl::Depth depth{};
  gl::BlendMode blend = gl::BlendMode::Alpha;
  gl::CullMode cull_mode = gl::CullMode::Back;
  gl::PolygonMode polygon_mode = gl::PolygonMode::Fill;
  ColorMask color_mask{};
  bool color_blend = true;
  bool wireframe = false;

  auto operator()(NonSendMarker) const -> void {
    gl::draw_render_batch_active();
    gl::color_mask(color_mask.r, color_mask.g, color_mask.b, color_mask.a);

    if (color_blend) {
      gl::enable_color_blend();
    } else {
      gl::disable_color_blend();
    }

    if (depth.test) {
      gl::enable_depth_test();
    } else {
      gl::disable_depth_test();
    }

    if (depth.write) {
      gl::enable_depth_mask();
    } else {
      gl::disable_depth_mask();
    }

    switch (cull_mode) {
    case gl::CullMode::Back:
      gl::enable_backface_culling();
      gl::set_cull_face(gl::CullFace::Back);
      break;
    case gl::CullMode::Front:
      gl::enable_backface_culling();
      gl::set_cull_face(gl::CullFace::Front);
      break;
    case gl::CullMode::None:
      gl::disable_backface_culling();
      break;
    }

    if (wireframe || polygon_mode == gl::PolygonMode::Line) {
      gl::enable_wire_mode();
    } else {
      gl::disable_wire_mode();
    }

    switch (blend) {
    case gl::BlendMode::Alpha:
      gl::set_blend_mode(RL_BLEND_ALPHA);
      break;
    case gl::BlendMode::Additive:
      gl::set_blend_mode(RL_BLEND_ADDITIVE);
      break;
    case gl::BlendMode::Multiplied:
      gl::set_blend_mode(RL_BLEND_MULTIPLIED);
      break;
    case gl::BlendMode::Custom:
      break;
    }
  }

  static auto get_default() -> RenderConfig { return {}; }

  static auto depth_prepass() -> RenderConfig {
    return {
        .depth = {.test = true, .write = true},
        .blend = gl::BlendMode::Alpha,
        .cull_mode = gl::CullMode::Back,
        .polygon_mode = gl::PolygonMode::Fill,
        .color_mask = {.r = false, .g = false, .b = false, .a = false},
        .color_blend = false,
    };
  }

  static auto post_depth_prepass() -> RenderConfig { return get_default(); }

  static auto opaque() -> RenderConfig {
    return {
        .depth = {.test = true, .write = true},
        .blend = gl::BlendMode::Alpha,
        .cull_mode = gl::CullMode::Back,
        .polygon_mode = gl::PolygonMode::Fill,
        .color_mask = {},
        .color_blend = false,
    };
  }

  static auto transparent() -> RenderConfig {
    return {
        .depth = {.test = true, .write = false},
        .blend = gl::BlendMode::Alpha,
        .cull_mode = gl::CullMode::Back,
        .polygon_mode = gl::PolygonMode::Fill,
        .color_mask = {},
        .color_blend = true,
    };
  }

  static auto end_world_pass() -> RenderConfig {
    return {
        .depth = {.test = false, .write = true},
        .blend = gl::BlendMode::Alpha,
        .cull_mode = gl::CullMode::None,
        .polygon_mode = gl::PolygonMode::Fill,
        .color_mask = {},
        .color_blend = true,
    };
  }
};

} // namespace ecs
