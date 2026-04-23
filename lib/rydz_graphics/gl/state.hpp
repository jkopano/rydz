#pragma once

#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <optional>

namespace ecs {
struct RenderConfig;
} // namespace ecs

namespace gl {

inline auto set_blend_mode(int mode) -> void { rl::rlSetBlendMode(mode); }

struct Depth {
  enum class Func {
    Always,
    Never,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
  };
  bool test = true;
  bool write = true;
  Func func = Func::Less;

  auto operator==(Depth const&) const -> bool = default;

  auto apply() const -> void {
    test ? rl::rlEnableDepthTest() : rl::rlDisableDepthTest();
    write ? rl::rlEnableDepthMask() : rl::rlDisableDepthMask();
  }
};

struct Blend {
  enum class Mode : u8 {
    Alpha = RL_BLEND_ALPHA,
    Additive = RL_BLEND_ADDITIVE,
    Multiplied = RL_BLEND_MULTIPLIED,
    AddColors = RL_BLEND_ADD_COLORS,
    SubtractColors = RL_BLEND_SUBTRACT_COLORS,
    AlphaPremultiply = RL_BLEND_ALPHA_PREMULTIPLY,
    Custom = RL_BLEND_CUSTOM,
    CustomSeparate = RL_BLEND_CUSTOM_SEPARATE,
  };
  Mode mode = Mode::Alpha;
  bool enabled{};

  static constexpr auto disabled() -> Blend {
    return {.mode = Mode::Alpha, .enabled = false};
  }
  static constexpr auto alpha() -> Blend {
    return {.mode = Mode::Alpha, .enabled = true};
  }
  static constexpr auto additive() -> Blend {
    return {.mode = Mode::Additive, .enabled = true};
  }
  static constexpr auto multiplied() -> Blend {
    return {.mode = Mode::Multiplied, .enabled = true};
  }

  auto operator==(Blend const&) const -> bool = default;

  auto apply() const -> void {
    if (!enabled) {
      rl::rlDisableColorBlend();
      return;
    }

    rl::rlEnableColorBlend();
    rl::rlSetBlendMode(static_cast<int>(mode));
  }

  auto enable() -> void { enabled = true; }
  auto disable() -> void { enabled = false; }
  static Blend const ALPHA;
  static Blend const ADD;
  static Blend const MULT;
  static Blend const CUSTOM;
};

inline constexpr Blend Blend::ALPHA{.mode = Mode::Alpha, .enabled = true};
inline constexpr Blend Blend::ADD{.mode = Mode::Additive, .enabled = true};
inline constexpr Blend Blend::MULT{.mode = Mode::Multiplied, .enabled = true};
inline constexpr Blend Blend::CUSTOM{.mode = Mode::Custom, .enabled = true};

struct Cull {
  enum class Face : int {
    Back = RL_CULL_FACE_BACK,
    Front = RL_CULL_FACE_FRONT,
  };

  bool enabled = true;
  Face face = Face::Back;

  static constexpr auto none() -> Cull {
    return {.enabled = false, .face = Face::Back};
  }
  static constexpr auto back() -> Cull {
    return {.enabled = true, .face = Face::Back};
  }
  static constexpr auto front() -> Cull {
    return {.enabled = true, .face = Face::Front};
  }

  auto operator==(Cull const&) const -> bool = default;

  auto apply() const -> void {
    if (!enabled) {
      rl::rlDisableBackfaceCulling();
      return;
    }

    rl::rlEnableBackfaceCulling();
    rl::rlSetCullFace(static_cast<int>(face));
  }
};

struct Polygon {
  enum class Mode {
    Fill,
    Line,
    Point,
  };
  Mode mode = Mode::Fill;

  static constexpr auto fill() -> Polygon { return {.mode = Mode::Fill}; }
  static constexpr auto line() -> Polygon { return {.mode = Mode::Line}; }
  static constexpr auto point() -> Polygon { return {.mode = Mode::Point}; }

  auto operator==(Polygon const&) const -> bool = default;

  auto apply() const -> void {
    if (mode == Mode::Line) {
      rl::rlEnableWireMode();
    } else {
      rl::rlDisableWireMode();
    }
  }
};

struct ColorMask {
  bool r = true;
  bool g = true;
  bool b = true;
  bool a = true;

  auto operator==(ColorMask const&) const -> bool = default;

  auto apply() const -> void { rl::rlColorMask(r, g, b, a); }
};

struct RenderViewState {
  Rectangle viewport{0.0F, 0.0F, 1.0F, 1.0F};
  math::Mat4 view = math::Mat4::IDENTITY;
  math::Mat4 projection = math::Mat4::IDENTITY;
  math::Vec3 camera_position{};
  bool orthographic = false;
  float near_plane = 0.1F;
  float far_plane = 1000.0F;
};

class RenderState {
public:
  using T = ecs::Resource;

  auto apply(ecs::RenderConfig const& config) -> void;

  auto set_depth(Depth depth) -> void {
    if (depth_ == depth) {
      return;
    }
    flush_batch();
    depth.apply();
    depth_ = depth;
  }

  auto set_blend(Blend blend) -> void {
    if (blend_ == blend) {
      return;
    }
    flush_batch();
    blend.apply();
    blend_ = blend;
  }

  auto set_cull(Cull cull) -> void {
    if (cull_ == cull) {
      return;
    }
    flush_batch();
    cull.apply();
    cull_ = cull;
  }

  auto set_color_mask(ColorMask mask) -> void {
    if (color_mask_ == mask) {
      return;
    }
    flush_batch();
    mask.apply();
    color_mask_ = mask;
  }

  auto set_polygon(Polygon polygon) -> void {
    if (polygon_ == polygon) {
      return;
    }
    flush_batch();
    polygon.apply();
    polygon_ = polygon;
  }

  auto set_shader(ShaderProgram& shader) -> void {
    u32 const id = shader.raw().id;
    if (shader_id_ == id) {
      return;
    }
    flush_batch();
    rl::rlSetShader(id, shader.raw().locs);
    shader_id_ = id;
  }

  auto clear_shader() -> void {
    if (shader_id_ == 0) {
      return;
    }
    flush_batch();
    rl::rlSetShader(rl::rlGetShaderIdDefault(), rl::rlGetShaderLocsDefault());
    shader_id_ = 0;
  }

  auto begin_target(RenderTarget& target) -> void {
    if (target_ == &target) {
      return;
    }
    flush_batch();
    target.begin();
    target_ = &target;
    reset();
  }

  auto end_target() -> void {
    if (target_ == nullptr) {
      return;
    }
    flush_batch();
    target_->end();
    target_ = nullptr;
    reset();
  }

  auto begin_view(RenderViewState view) -> void {
    if (view_active_) {
      end_view();
    }

    flush_batch();
    view_ = view;
    view_active_ = true;

    rl::rlMatrixMode(RL_PROJECTION);
    rl::rlPushMatrix();
    rl::rlLoadIdentity();
    rl::rlSetMatrixProjection(math::to_rl(view_.projection));

    rl::rlMatrixMode(RL_MODELVIEW);
    rl::rlPushMatrix();
    rl::rlLoadIdentity();
    rl::rlSetMatrixModelview(math::to_rl(view_.view));
  }

  auto end_view() -> void {
    if (!view_active_) {
      return;
    }

    flush_batch();
    rl::rlMatrixMode(RL_MODELVIEW);
    rl::rlPopMatrix();
    rl::rlMatrixMode(RL_PROJECTION);
    rl::rlPopMatrix();
    rl::rlMatrixMode(RL_MODELVIEW);
    view_active_ = false;
  }

  [[nodiscard]] auto view() const -> RenderViewState const& { return view_; }

  [[nodiscard]] auto view_active() const -> bool { return view_active_; }

  auto flush_batch() const -> void { rl::rlDrawRenderBatchActive(); }

  auto reset() -> void {
    clear_shader();
    depth_.reset();
    blend_.reset();
    cull_.reset();
    polygon_.reset();
    color_mask_.reset();
  }

private:
  std::optional<Depth> depth_;
  std::optional<Blend> blend_;
  std::optional<Cull> cull_;
  std::optional<Polygon> polygon_;
  std::optional<ColorMask> color_mask_;
  u32 shader_id_ = 0;
  RenderTarget* target_ = nullptr;
  RenderViewState view_{};
  bool view_active_ = false;
};

inline auto draw_fps(int x, int y) -> void { rl::DrawFPS(x, y); }

inline auto draw_texture_pro(
  Texture const& texture,
  Rectangle source,
  Rectangle dest,
  Vec2 origin,
  float rotation,
  ecs::Color tint
) -> void {
  rl::DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

} // namespace gl
