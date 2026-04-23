#pragma once

#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/gl/textures.hpp"

namespace gl {

class DrawContext {
public:
  explicit DrawContext(RenderState& state) noexcept : state_(state) {}

  DrawContext(DrawContext const&) = delete;
  auto operator=(DrawContext const&) -> DrawContext& = delete;

  auto set_depth(Depth d) -> void { state_.set_depth(d); }
  auto set_blend(Blend b) -> void { state_.set_blend(b); }
  auto set_cull(Cull c) -> void { state_.set_cull(c); }
  auto set_polygon(Polygon p) -> void { state_.set_polygon(p); }
  auto set_color_mask(ColorMask m) -> void { state_.set_color_mask(m); }
  auto set_shader(ShaderProgram& s) -> void { state_.set_shader(s); }
  auto clear_shader() -> void { state_.clear_shader(); }

  auto begin_view(RenderViewState v) -> void { state_.begin_view(v); }
  auto end_view() -> void { state_.end_view(); }

  auto clear(ecs::Color color) -> void { rl::ClearBackground(color); }
  auto flush() -> void { state_.flush_batch(); }
  auto draw_fps(int x, int y) -> void { rl::DrawFPS(x, y); }
  auto draw_texture(
    Texture const& texture,
    Rectangle src,
    Rectangle dst,
    Vec2 origin,
    float rotation,
    ecs::Color tint
  ) -> void {
    rl::DrawTexturePro(texture, src, dst, origin, rotation, tint);
  }

  auto set_vertex_attribute(
    u32 index, i32 components, i32 type, bool normalized, i32 stride, i32 offset
  ) -> void {
    rl::rlSetVertexAttribute(index, components, type, normalized, stride, offset);
  }
  auto enable_vertex_attribute(u32 index) -> void { rl::rlEnableVertexAttribute(index); }

  auto active_texture_slot(i32 slot) -> void { rl::rlActiveTextureSlot(slot); }

  [[nodiscard]] auto raw_state() -> RenderState& { return state_; }

private:
  RenderState& state_;
};

} // namespace gl
