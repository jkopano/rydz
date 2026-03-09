#pragma once
#include "rydz_ecs/asset.hpp"
#include <concepts>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

namespace ecs {

template <typename M>
concept IMaterial =
    requires(const M m, Model &model, Assets<Texture2D> *textures) {
      { m.color() } -> std::convertible_to<Color>;
      { m.apply(model, textures) } -> std::same_as<void>;
      { m.vertex_source() } -> std::convertible_to<const char *>;
      { m.fragment_source() } -> std::convertible_to<const char *>;
      { m.shader() } -> std::convertible_to<const Shader *>;
    };

template <IMaterial M> inline void preload_material_shader(NonSendMarker) {
  M{}.shader();
}

struct StandardMaterial {
  Color base_color = WHITE;
  Handle<Texture2D> texture{};
  Handle<Texture2D> normal_map{};
  float metallic = 0.0f;
  float roughness = 0.5f;

  static StandardMaterial from_color(Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<Texture2D> tex,
                                       Color tint = WHITE) {
    return {.base_color = tint, .texture = tex};
  }

  Color color() const { return base_color; }

  void apply(Model &model, Assets<Texture2D> *tex_assets) const {
    Shader &shader = standard_shader();
    if (model.materials) {
      for (int i = 0; i < model.materialCount; ++i) {
        model.materials[i].shader = shader;
      }
    }
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = base_color;

    if (tex_assets && texture.is_valid()) {
      Texture2D *tex = tex_assets->get(texture);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
    }

    if (tex_assets && normal_map.is_valid()) {
      Texture2D *tex = tex_assets->get(normal_map);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_NORMAL].texture = *tex;
    }
  }

  // Shader paths — nullptr means use raylib's default shader
  const char *vertex_source() const { return "res/shaders/pbr.vert"; }
  const char *fragment_source() const { return "res/shaders/pbr.frag"; }

  const Shader *shader() const { return &standard_shader(); }

private:
  static Shader &standard_shader() {
    static Shader shader = {};
    static bool loaded = false;
    if (!loaded) {
      shader = LoadShader("res/shaders/pbr.vert", "res/shaders/pbr.frag");

      if (shader.locs == nullptr) {
        shader.locs = (int *)RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int));
        for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i)
          shader.locs[i] = -1;
      }

      shader.locs[SHADER_LOC_VERTEX_POSITION] =
          GetShaderLocationAttrib(shader, "vertexPosition");
      shader.locs[SHADER_LOC_VERTEX_NORMAL] =
          GetShaderLocationAttrib(shader, "vertexNormal");
      shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
          GetShaderLocationAttrib(shader, "vertexTexCoord");
      shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
      shader.locs[SHADER_LOC_COLOR_DIFFUSE] =
          GetShaderLocation(shader, "colDiffuse");
      shader.locs[SHADER_LOC_MAP_DIFFUSE] =
          GetShaderLocation(shader, "texture0");
      shader.locs[SHADER_LOC_MAP_NORMAL] =
          GetShaderLocation(shader, "u_normal_texture");
      shader.locs[SHADER_LOC_MATRIX_MODEL] =
          GetShaderLocationAttrib(shader, "instanceTransform");

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
  explicit MeshMaterial3d(Color c)
    requires std::constructible_from<M, Color>
      : material{c} {}
};

using Material3d = MeshMaterial3d<StandardMaterial>;

} // namespace ecs
