#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <concepts>

namespace ecs {

template <typename M>
concept IMaterial =
    requires(const M m, rl::Model &model, Assets<rl::Texture2D> *textures) {
      { m.color() } -> std::convertible_to<rl::Color>;
      { m.apply(model, textures) } -> std::same_as<void>;
      { M::vertex_source() } -> std::convertible_to<const char *>;
      { M::fragment_source() } -> std::convertible_to<const char *>;
      { M::shader() } -> std::convertible_to<const rl::Shader *>;
    };

struct StandardMaterial {
  rl::Color base_color = WHITE;
  Handle<rl::Texture2D> texture{};
  Handle<rl::Texture2D> normal_map{};
  Handle<rl::Texture2D> metallic_map{};
  Handle<rl::Texture2D> roughness_map{};
  Handle<rl::Texture2D> occlusion_map{};
  Handle<rl::Texture2D> emissive_map{};
  rl::Color emissive_color = {0, 0, 0, 0};
  float metallic = -1.0f;
  float roughness = -1.0f;
  float normal_scale = -1.0f;
  float occlusion_strength = -1.0f;

  static StandardMaterial from_color(rl::Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<rl::Texture2D> tex,
                                       rl::Color tint = WHITE) {
    return {.base_color = tint, .texture = tex};
  }

  rl::Color color() const { return base_color; }

  void apply(rl::Model &model, Assets<rl::Texture2D> *tex_assets) const {
    const rl::Shader *s = shader();
    if (!model.materials || model.materialCount <= 0) {
      return;
    }

    for (int i = 0; i < model.materialCount; ++i) {
      auto &material = model.materials[i];
      if (s) {
        material.shader = *s;
      }

      material.maps[MATERIAL_MAP_DIFFUSE].color = base_color;

      if (metallic >= 0.0f) {
        material.maps[MATERIAL_MAP_METALNESS].value = metallic;
      }
      if (roughness >= 0.0f) {
        material.maps[MATERIAL_MAP_ROUGHNESS].value = roughness;
      }
      if (normal_scale >= 0.0f) {
        material.maps[MATERIAL_MAP_NORMAL].value = normal_scale;
      }
      if (occlusion_strength >= 0.0f) {
        material.maps[MATERIAL_MAP_OCCLUSION].value = occlusion_strength;
      }
      if (emissive_color.a > 0) {
        material.maps[MATERIAL_MAP_EMISSION].color = emissive_color;
      }

      if (!tex_assets) {
        continue;
      }

      auto assign_map = [&](int map_type, Handle<rl::Texture2D> handle) {
        if (!handle.is_valid()) {
          return;
        }
        if (rl::Texture2D *tex = tex_assets->get(handle)) {
          material.maps[map_type].texture = *tex;
        }
      };

      assign_map(MATERIAL_MAP_DIFFUSE, texture);
      assign_map(MATERIAL_MAP_NORMAL, normal_map);
      assign_map(MATERIAL_MAP_METALNESS, metallic_map);
      assign_map(MATERIAL_MAP_ROUGHNESS, roughness_map);
      assign_map(MATERIAL_MAP_OCCLUSION, occlusion_map);
      assign_map(MATERIAL_MAP_EMISSION, emissive_map);
    }
  }

  static const char *vertex_source() { return "res/shaders/pbr.vert"; }
  static const char *fragment_source() { return "res/shaders/pbr.frag"; }

  static const rl::Shader *shader() {
    static rl::Shader s = [] {
      rl::Shader sh =
          rl::LoadShader("res/shaders/pbr.vert", "res/shaders/pbr.frag");

      if (sh.locs == nullptr) {
        sh.locs = (int *)RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int));
        for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i)
          sh.locs[i] = -1;
      }

      sh.locs[SHADER_LOC_VERTEX_POSITION] =
          rl::GetShaderLocationAttrib(sh, "vertexPosition");
      sh.locs[SHADER_LOC_VERTEX_NORMAL] =
          rl::GetShaderLocationAttrib(sh, "vertexNormal");
      sh.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
          rl::GetShaderLocationAttrib(sh, "vertexTexCoord");
      sh.locs[SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(sh, "mvp");
      sh.locs[SHADER_LOC_COLOR_DIFFUSE] =
          rl::GetShaderLocation(sh, "colDiffuse");
      sh.locs[SHADER_LOC_MAP_DIFFUSE] = rl::GetShaderLocation(sh, "texture0");
      sh.locs[SHADER_LOC_MAP_METALNESS] =
          rl::GetShaderLocation(sh, "u_metallic_texture");
      sh.locs[SHADER_LOC_MAP_NORMAL] =
          rl::GetShaderLocation(sh, "u_normal_texture");
      sh.locs[SHADER_LOC_MAP_ROUGHNESS] =
          rl::GetShaderLocation(sh, "u_roughness_texture");
      sh.locs[SHADER_LOC_MAP_OCCLUSION] =
          rl::GetShaderLocation(sh, "u_occlusion_texture");
      sh.locs[SHADER_LOC_MAP_EMISSION] =
          rl::GetShaderLocation(sh, "u_emissive_texture");
      sh.locs[SHADER_LOC_MATRIX_MODEL] = rl::GetShaderLocation(sh, "matModel");
      sh.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
          rl::GetShaderLocationAttrib(sh, "instanceTransform");
      return sh;
    }();
    return &s;
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
