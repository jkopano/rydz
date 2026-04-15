#pragma once

#include "rydz_graphics/gl/core.hpp"
#include <algorithm>

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

inline int backend_cull_face(CullFace face) {
  switch (face) {
  case CullFace::Back:
    return RL_CULL_FACE_BACK;
  case CullFace::Front:
    return RL_CULL_FACE_FRONT;
  }
  return RL_CULL_FACE_BACK;
}

inline void set_cull_face(CullFace face) {
  rl::rlSetCullFace(backend_cull_face(face));
}

enum class PolygonMode {
  Fill,
  Line,
  Point,
};

inline int screen_width() { return rl::GetScreenWidth(); }

inline int screen_height() { return rl::GetScreenHeight(); }

inline void begin_drawing() { rl::BeginDrawing(); }

inline void end_drawing() { rl::EndDrawing(); }

inline void clear_background(Color color) { rl::ClearBackground(color); }

inline void begin_texture_mode(RenderTarget target) { target.begin(); }

inline void draw_fps(int x, int y) { rl::DrawFPS(x, y); }

inline void draw_render_batch_active() { rl::rlDrawRenderBatchActive(); }

inline void matrix_mode(int mode) { rl::rlMatrixMode(mode); }

inline void push_matrix() { rl::rlPushMatrix(); }

inline void pop_matrix() { rl::rlPopMatrix(); }

inline void load_identity() { rl::rlLoadIdentity(); }

inline void set_projection_matrix(math::Mat4 projection) {
  rl::rlSetMatrixProjection(math::to_rl(projection));
}

inline void set_modelview_matrix(math::Mat4 modelview) {
  rl::rlSetMatrixModelview(math::to_rl(modelview));
}

inline void enable_depth_test() { rl::rlEnableDepthTest(); }

inline void disable_depth_test() { rl::rlDisableDepthTest(); }

inline void enable_depth_mask() { rl::rlEnableDepthMask(); }

inline void disable_depth_mask() { rl::rlDisableDepthMask(); }

inline void enable_backface_culling() { rl::rlEnableBackfaceCulling(); }

inline void disable_backface_culling() { rl::rlDisableBackfaceCulling(); }

inline void enable_color_blend() { rl::rlEnableColorBlend(); }

inline void disable_color_blend() { rl::rlDisableColorBlend(); }

inline void enable_wire_mode() { rl::rlEnableWireMode(); }

inline void disable_wire_mode() { rl::rlDisableWireMode(); }

inline void set_blend_mode(int mode) { rl::rlSetBlendMode(mode); }

inline void color_mask(bool r, bool g, bool b, bool a) {
  rl::rlColorMask(r, g, b, a);
}

inline void begin_world_pass(math::Mat4 view, math::Mat4 projection) {
  draw_render_batch_active();
  matrix_mode(RL_PROJECTION);
  push_matrix();
  load_identity();
  set_projection_matrix(projection);
  matrix_mode(RL_MODELVIEW);
  load_identity();
  set_modelview_matrix(view);
}

inline void end_world_pass() {
  draw_render_batch_active();
  matrix_mode(RL_PROJECTION);
  pop_matrix();
  matrix_mode(RL_MODELVIEW);
  load_identity();
}

inline Rectangle texture_rect(const Texture &texture) { return texture.rect(); }

inline Rectangle flipped_texture_rect(const Texture &texture) {
  return texture.flipped_rect();
}

inline Rectangle screen_rect() {
  return {0.0f, 0.0f, static_cast<float>(screen_width()),
          static_cast<float>(screen_height())};
}

inline void draw_texture_pro(const Texture &texture, Rectangle source,
                             Rectangle dest, Vec2 origin, float rotation,
                             Color tint) {
  rl::DrawTexturePro(texture, source, dest, origin, rotation, tint);
}

inline void draw_mesh_instanced(const Mesh &mesh, Material &material,
                                const Matrix *transforms, i32 count) {
  mesh.draw_instanced(material, transforms, count);
}

} // namespace gl
