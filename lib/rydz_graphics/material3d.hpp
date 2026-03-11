#pragma once
#include "rydz_ecs/asset.hpp"
#include "rl.hpp"
#include <concepts>

namespace ecs {

template <typename M>
concept IMaterial =
    requires(const M m, rl::Model &model, Assets<rl::Texture2D> *textures) {
      { m.color() } -> std::convertible_to<rl::Color>;
      { m.apply(model, textures) } -> std::same_as<void>;
      { m.vertex_source() } -> std::convertible_to<const char *>;
      { m.fragment_source() } -> std::convertible_to<const char *>;
      { m.shader() } -> std::convertible_to<const rl::Shader *>;
    };

template <IMaterial M> inline void preload_material_shader(NonSendMarker) {
  M{}.shader();
}

struct StandardMaterial {
  rl::Color base_color = WHITE;
  Handle<rl::Texture2D> texture{};
  Handle<rl::Texture2D> normal_map{};
  float metallic = 0.0f;
  float roughness = 0.5f;

  static StandardMaterial from_color(rl::Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<rl::Texture2D> tex,
                                       rl::Color tint = WHITE) {
    return {.base_color = tint, .texture = tex};
  }

  rl::Color color() const { return base_color; }

  void apply(rl::Model &model, Assets<rl::Texture2D> *tex_assets) const {
    rl::Shader &shader = standard_shader();
    if (model.materials) {
      for (int i = 0; i < model.materialCount; ++i) {
        model.materials[i].shader = shader;
      }
    }
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = base_color;

    if (tex_assets && texture.is_valid()) {
      rl::Texture2D *tex = tex_assets->get(texture);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
    }

    if (tex_assets && normal_map.is_valid()) {
      rl::Texture2D *tex = tex_assets->get(normal_map);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_NORMAL].texture = *tex;
    }
  }

  // Shader paths — nullptr means use raylib's default shader
  const char *vertex_source() const { return "res/shaders/pbr.vert"; }
  const char *fragment_source() const { return "res/shaders/pbr.frag"; }

  const rl::Shader *shader() const { return &standard_shader(); }

private:
  static rl::Shader &standard_shader() {
    static rl::Shader shader = {};
    static bool loaded = false;
    if (!loaded) {
      shader = rl::LoadShader("res/shaders/pbr.vert", "res/shaders/pbr.frag");

      if (shader.locs == nullptr) {
        shader.locs = (int *)RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int));
        for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i)
          shader.locs[i] = -1;
      }

      shader.locs[SHADER_LOC_VERTEX_POSITION] =
          rl::GetShaderLocationAttrib(shader, "vertexPosition");
      shader.locs[SHADER_LOC_VERTEX_NORMAL] =
          rl::GetShaderLocationAttrib(shader, "vertexNormal");
      shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
          rl::GetShaderLocationAttrib(shader, "vertexTexCoord");
      shader.locs[SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(shader, "mvp");
      shader.locs[SHADER_LOC_COLOR_DIFFUSE] =
          rl::GetShaderLocation(shader, "colDiffuse");
      shader.locs[SHADER_LOC_MAP_DIFFUSE] =
          rl::GetShaderLocation(shader, "texture0");
      shader.locs[SHADER_LOC_MAP_NORMAL] =
          rl::GetShaderLocation(shader, "u_normal_texture");
      shader.locs[SHADER_LOC_MATRIX_MODEL] =
          rl::GetShaderLocationAttrib(shader, "instanceTransform");

      loaded = true;
    }
    return shader;
  }
};

static_assert(IMaterial<StandardMaterial>);

template <IMaterial M> struct MeshMaterial3d {
  M material;

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(M m) : material(std::move(m)) {}
  explicit MeshMaterial3d(rl::Color c)
    requires std::constructible_from<M, rl::Color>
      : material{c} {}
};

using Material3d = MeshMaterial3d<StandardMaterial>;

} // namespace ecs
