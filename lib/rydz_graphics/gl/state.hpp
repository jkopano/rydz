#pragma once

#include "rydz_graphics/gl/core.hpp"

namespace gl {

enum class DepthFunc {
  Always,
  Never,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Equal,
  NotEqual,
};

struct Depth {
  bool test = true;
  bool write = true;
  DepthFunc func = DepthFunc::Less;
};

enum class BlendMode {
  Alpha,
  Additive,
  Multiplied,
  Custom,
};

enum class CullMode {
  None,
  Back,
  Front,
};

enum class CullFace {
  Back,
  Front,
};

inline auto backend_cull_face(CullFace face) -> int {
  switch (face) {
  case CullFace::Back:
    return RL_CULL_FACE_BACK;
  case CullFace::Front:
    return RL_CULL_FACE_FRONT;
  }
  return RL_CULL_FACE_BACK;
}

inline auto set_cull_face(CullFace face) -> void {
  rl::rlSetCullFace(backend_cull_face(face));
}

enum class PolygonMode {
  Fill,
  Line,
  Point,
};

inline auto screen_width() -> int { return rl::GetScreenWidth(); }
inline auto screen_height() -> int { return rl::GetScreenHeight(); }
inline auto begin_drawing() -> void { rl::BeginDrawing(); }
inline auto end_drawing() -> void { rl::EndDrawing(); }
inline auto clear_background(Color color) -> void {
  rl::ClearBackground(color);
}

inline auto begin_texture_mode(RenderTarget target) -> void { target.begin(); }
inline auto draw_fps(int x, int y) -> void { rl::DrawFPS(x, y); }

inline auto draw_render_batch_active() -> void {
  rl::rlDrawRenderBatchActive();
}

inline auto matrix_mode(int mode) -> void { rl::rlMatrixMode(mode); }
inline auto push_matrix() -> void { rl::rlPushMatrix(); }
inline auto pop_matrix() -> void { rl::rlPopMatrix(); }
inline auto load_identity() -> void { rl::rlLoadIdentity(); }

inline auto set_projection_matrix(math::Mat4 projection) -> void {
  rl::rlSetMatrixProjection(math::to_rl(projection));
}

inline auto set_modelview_matrix(math::Mat4 modelview) -> void {
  rl::rlSetMatrixModelview(math::to_rl(modelview));
}

inline auto enable_depth_test() -> void { rl::rlEnableDepthTest(); }
inline auto disable_depth_test() -> void { rl::rlDisableDepthTest(); }
inline auto enable_depth_mask() -> void { rl::rlEnableDepthMask(); }
inline auto disable_depth_mask() -> void { rl::rlDisableDepthMask(); }
inline auto enable_backface_culling() -> void { rl::rlEnableBackfaceCulling(); }
inline auto disable_backface_culling() -> void {
  rl::rlDisableBackfaceCulling();
}

inline auto enable_color_blend() -> void { rl::rlEnableColorBlend(); }
inline auto disable_color_blend() -> void { rl::rlDisableColorBlend(); }
inline auto enable_wire_mode() -> void { rl::rlEnableWireMode(); }
inline auto disable_wire_mode() -> void { rl::rlDisableWireMode(); }
inline auto set_blend_mode(int mode) -> void { rl::rlSetBlendMode(mode); }
inline auto color_mask(bool r, bool g, bool b, bool a) -> void {
  rl::rlColorMask(r, g, b, a);
}

inline auto begin_world_pass(math::Mat4 view, math::Mat4 projection) -> void {
  draw_render_batch_active();
  matrix_mode(RL_PROJECTION);
  push_matrix();
  load_identity();
  set_projection_matrix(projection);
  matrix_mode(RL_MODELVIEW);
  load_identity();
  set_modelview_matrix(view);
}

inline auto end_world_pass() -> void {
  draw_render_batch_active();
  matrix_mode(RL_PROJECTION);
  pop_matrix();
  matrix_mode(RL_MODELVIEW);
  load_identity();
}

inline auto screen_rect() -> Rectangle {
  return {
    0.0f,
    0.0f,
    static_cast<float>(screen_width()),
    static_cast<float>(screen_height())
  };
}

inline auto draw_texture_pro(
  Texture const& texture,
  Rectangle source,
  Rectangle dest,
  Vec2 origin,
  float rotation,
  Color tint
) -> void {
  rl::DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

} // namespace gl
