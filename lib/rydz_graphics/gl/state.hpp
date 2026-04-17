#pragma once

#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <algorithm>
#include <array>
#include <cmath>

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

inline void draw_fps(int, int) {}

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
  if (texture.id == 0) {
    return;
  }

  const float width = static_cast<float>(texture.width);
  const float height = static_cast<float>(texture.height);
  bool flip_x = false;

  if (source.width < 0.0f) {
    flip_x = true;
    source.width *= -1.0f;
  }
  if (source.height < 0.0f) {
    source.y -= source.height;
  }
  if (dest.width < 0.0f) {
    dest.width *= -1.0f;
  }
  if (dest.height < 0.0f) {
    dest.height *= -1.0f;
  }

  Vec2 top_left{};
  Vec2 top_right{};
  Vec2 bottom_left{};
  Vec2 bottom_right{};

  if (rotation == 0.0f) {
    const float x = dest.x - origin.x;
    const float y = dest.y - origin.y;
    top_left = {x, y};
    top_right = {x + dest.width, y};
    bottom_left = {x, y + dest.height};
    bottom_right = {x + dest.width, y + dest.height};
  } else {
    constexpr float deg_to_rad = 3.14159265358979323846f / 180.0f;
    const float radians = rotation * deg_to_rad;
    const float sin_rotation = std::sin(radians);
    const float cos_rotation = std::cos(radians);
    const float x = dest.x;
    const float y = dest.y;
    const float dx = -origin.x;
    const float dy = -origin.y;

    top_left.x = x + dx * cos_rotation - dy * sin_rotation;
    top_left.y = y + dx * sin_rotation + dy * cos_rotation;
    top_right.x = x + (dx + dest.width) * cos_rotation - dy * sin_rotation;
    top_right.y = y + (dx + dest.width) * sin_rotation + dy * cos_rotation;
    bottom_left.x = x + dx * cos_rotation - (dy + dest.height) * sin_rotation;
    bottom_left.y = y + dx * sin_rotation + (dy + dest.height) * cos_rotation;
    bottom_right.x =
        x + (dx + dest.width) * cos_rotation -
        (dy + dest.height) * sin_rotation;
    bottom_right.y =
        y + (dx + dest.width) * sin_rotation +
        (dy + dest.height) * cos_rotation;
  }

  const float u0 = source.x / width;
  const float u1 = (source.x + source.width) / width;
  const float v0 = source.y / height;
  const float v1 = (source.y + source.height) / height;
  const float left_u = flip_x ? u1 : u0;
  const float right_u = flip_x ? u0 : u1;

  rl::rlSetTexture(texture.id);
  rl::rlBegin(RL_QUADS);
  rl::rlColor4ub(tint.r, tint.g, tint.b, tint.a);
  rl::rlNormal3f(0.0f, 0.0f, 1.0f);

  rl::rlTexCoord2f(left_u, v0);
  rl::rlVertex2f(top_left.x, top_left.y);

  rl::rlTexCoord2f(left_u, v1);
  rl::rlVertex2f(bottom_left.x, bottom_left.y);

  rl::rlTexCoord2f(right_u, v1);
  rl::rlVertex2f(bottom_right.x, bottom_right.y);

  rl::rlTexCoord2f(right_u, v0);
  rl::rlVertex2f(top_right.x, top_right.y);

  rl::rlEnd();
  rl::rlSetTexture(0);
}

inline Matrix matrix_multiply(Matrix left, Matrix right) {
  Matrix result{};
  result.m0 = left.m0 * right.m0 + left.m1 * right.m4 +
              left.m2 * right.m8 + left.m3 * right.m12;
  result.m1 = left.m0 * right.m1 + left.m1 * right.m5 +
              left.m2 * right.m9 + left.m3 * right.m13;
  result.m2 = left.m0 * right.m2 + left.m1 * right.m6 +
              left.m2 * right.m10 + left.m3 * right.m14;
  result.m3 = left.m0 * right.m3 + left.m1 * right.m7 +
              left.m2 * right.m11 + left.m3 * right.m15;
  result.m4 = left.m4 * right.m0 + left.m5 * right.m4 +
              left.m6 * right.m8 + left.m7 * right.m12;
  result.m5 = left.m4 * right.m1 + left.m5 * right.m5 +
              left.m6 * right.m9 + left.m7 * right.m13;
  result.m6 = left.m4 * right.m2 + left.m5 * right.m6 +
              left.m6 * right.m10 + left.m7 * right.m14;
  result.m7 = left.m4 * right.m3 + left.m5 * right.m7 +
              left.m6 * right.m11 + left.m7 * right.m15;
  result.m8 = left.m8 * right.m0 + left.m9 * right.m4 +
              left.m10 * right.m8 + left.m11 * right.m12;
  result.m9 = left.m8 * right.m1 + left.m9 * right.m5 +
              left.m10 * right.m9 + left.m11 * right.m13;
  result.m10 = left.m8 * right.m2 + left.m9 * right.m6 +
               left.m10 * right.m10 + left.m11 * right.m14;
  result.m11 = left.m8 * right.m3 + left.m9 * right.m7 +
               left.m10 * right.m11 + left.m11 * right.m15;
  result.m12 = left.m12 * right.m0 + left.m13 * right.m4 +
               left.m14 * right.m8 + left.m15 * right.m12;
  result.m13 = left.m12 * right.m1 + left.m13 * right.m5 +
               left.m14 * right.m9 + left.m15 * right.m13;
  result.m14 = left.m12 * right.m2 + left.m13 * right.m6 +
               left.m14 * right.m10 + left.m15 * right.m14;
  result.m15 = left.m12 * right.m3 + left.m13 * right.m7 +
               left.m14 * right.m11 + left.m15 * right.m15;
  return result;
}

inline Vec4 color_to_vec4(Color color) {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f,
          color.a / 255.0f};
}

inline bool is_cubemap_material_slot(ecs::MaterialMap map) {
  return map == ecs::MaterialMap::Cubemap ||
         map == ecs::MaterialMap::Irradiance ||
         map == ecs::MaterialMap::Prefilter;
}

inline constexpr std::array<ecs::MaterialMap, 11> kMaterialMapSlots = {
    ecs::MaterialMap::Albedo,   ecs::MaterialMap::Metalness,
    ecs::MaterialMap::Normal,   ecs::MaterialMap::Roughness,
    ecs::MaterialMap::Occlusion, ecs::MaterialMap::Emission,
    ecs::MaterialMap::Height,   ecs::MaterialMap::Cubemap,
    ecs::MaterialMap::Irradiance, ecs::MaterialMap::Prefilter,
    ecs::MaterialMap::Brdf,
};

inline void bind_material_textures(ShaderProgram &shader, Material &material) {
  for (ecs::MaterialMap map : kMaterialMapSlots) {
    const int slot = ecs::material_map_index(map);
    const Texture &texture = material.maps[slot].texture;
    if (texture.id == 0) {
      continue;
    }

    rl::rlActiveTextureSlot(slot);
    if (is_cubemap_material_slot(map)) {
      rl::rlEnableTextureCubemap(texture.id);
    } else {
      rl::rlEnableTexture(texture.id);
    }
    shader.set(ecs::map_texture_binding(map), slot);
  }
}

inline void unbind_material_textures(Material &material) {
  for (ecs::MaterialMap map : kMaterialMapSlots) {
    const int slot = ecs::material_map_index(map);
    if (material.maps[slot].texture.id == 0) {
      continue;
    }

    rl::rlActiveTextureSlot(slot);
    if (is_cubemap_material_slot(map)) {
      rl::rlDisableTextureCubemap();
    } else {
      rl::rlDisableTexture();
    }
  }
}

inline void draw_mesh_instanced(ShaderProgram &shader, const Mesh &mesh,
                                Material &material,
                                const Matrix *transforms, i32 count) {
  if (count <= 0 || !mesh.uploaded() || mesh.vboId == nullptr) {
    return;
  }

  draw_render_batch_active();
  rl::rlEnableShader(shader.raw().id);

  const int albedo_index = ecs::material_map_index(ecs::MaterialMap::Albedo);
  shader.set("colDiffuse", color_to_vec4(material.maps[albedo_index].color));

  const Matrix modelview = rl::rlGetMatrixModelview();
  const Matrix projection = rl::rlGetMatrixProjection();
  shader.set("mvp", matrix_multiply(modelview, projection));

  bind_material_textures(shader, material);

  const u32 instance_vbo =
      rl::rlLoadVertexBuffer(transforms, count * static_cast<i32>(sizeof(Matrix)),
                             false);
  if (instance_vbo == 0 || !rl::rlEnableVertexArray(mesh.vaoId)) {
    if (instance_vbo != 0) {
      rl::rlUnloadVertexBuffer(instance_vbo);
    }
    unbind_material_textures(material);
    rl::rlDisableShader();
    return;
  }

  const int instance_location = shader.attribute_location("instanceTransform");
  if (instance_location >= 0) {
    rl::rlEnableVertexBuffer(instance_vbo);
    for (u32 i = 0; i < 4; ++i) {
      const u32 attribute = static_cast<u32>(instance_location) + i;
      rl::rlEnableVertexAttribute(attribute);
      rl::rlSetVertexAttribute(attribute, 4, RL_FLOAT, false, sizeof(Matrix),
                               static_cast<i32>(i * sizeof(Vec4)));
      rl::rlSetVertexAttributeDivisor(attribute, 1);
    }
  }

  if (mesh.indices != nullptr) {
    rl::rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount * 3, nullptr,
                                           count);
  } else {
    rl::rlDrawVertexArrayInstanced(0, mesh.vertexCount, count);
  }

  rl::rlDisableVertexArray();
  rl::rlDisableVertexBuffer();
  rl::rlUnloadVertexBuffer(instance_vbo);
  unbind_material_textures(material);
  rl::rlDisableShader();
}

} // namespace gl
