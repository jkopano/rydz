#pragma once
#include "clustered_lighting.hpp"
#include "render_batches.hpp"
#include "render_extract.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace ecs {

struct PreparedMaterial {
  std::array<rl::MaterialMap, 12> local_maps{};
  rl::Material material{};
};

struct PbrFallbackTextures {
  rl::Texture2D metallic_black{};
  rl::Texture2D roughness_white{};
  rl::Texture2D normal_flat{};
  rl::Texture2D occlusion_white{};
  rl::Texture2D emission_black{};
};

inline bool has_texture(const rl::Texture2D &texture) { return texture.id > 0; }

inline MaterialKey material_key(const StandardMaterial &material) {
  MaterialKey key{};
  key.base_color = material.base_color;
  key.emissive_color = material.emissive_color;
  key.texture_id = material.texture.id;
  key.normal_id = material.normal_map.id;
  key.metallic_id = material.metallic_map.id;
  key.roughness_id = material.roughness_map.id;
  key.occlusion_id = material.occlusion_map.id;
  key.emissive_id = material.emissive_map.id;
  key.metallic = material.metallic;
  key.roughness = material.roughness;
  key.normal_scale = material.normal_scale;
  key.occlusion_strength = material.occlusion_strength;
  key.shader = material.shader();
  return key;
}

inline rl::Material &fallback_material(NonSendMarker) {
  static rl::Material fallback = {};
  static bool init = false;
  if (!init) {
    fallback.shader.id = rl::rlGetShaderIdDefault();
    fallback.shader.locs = rl::rlGetShaderLocsDefault();
    fallback.maps = (rl::MaterialMap *)RL_CALLOC(12, sizeof(rl::MaterialMap));
    fallback.maps[MATERIAL_MAP_DIFFUSE].texture.id =
        rl::rlGetTextureIdDefault();
    fallback.maps[MATERIAL_MAP_DIFFUSE].texture.width = 1;
    fallback.maps[MATERIAL_MAP_DIFFUSE].texture.height = 1;
    fallback.maps[MATERIAL_MAP_DIFFUSE].texture.mipmaps = 1;
    fallback.maps[MATERIAL_MAP_DIFFUSE].texture.format =
        PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    fallback.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    init = true;
  }
  return fallback;
}

inline PbrFallbackTextures &pbr_fallback_textures() {
  static PbrFallbackTextures textures = [] {
    auto make_texture = [](rl::Color color) {
      rl::Image image = rl::GenImageColor(1, 1, color);
      rl::Texture2D texture = rl::LoadTextureFromImage(image);
      rl::UnloadImage(image);
      return texture;
    };

    return PbrFallbackTextures{
        .metallic_black = make_texture({0, 0, 0, 255}),
        .roughness_white = make_texture(WHITE),
        .normal_flat = make_texture({128, 128, 255, 255}),
        .occlusion_white = make_texture(WHITE),
        .emission_black = make_texture({0, 0, 0, 255}),
    };
  }();
  return textures;
}

inline void apply_pbr_defaults(rl::Material &material) {
  if (material.maps[MATERIAL_MAP_DIFFUSE].color.a == 0) {
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  }
  if (material.maps[MATERIAL_MAP_ROUGHNESS].value <= 0.0f &&
      !has_texture(material.maps[MATERIAL_MAP_ROUGHNESS].texture)) {
    material.maps[MATERIAL_MAP_ROUGHNESS].value = 1.0f;
  }
  if (material.maps[MATERIAL_MAP_NORMAL].value <= 0.0f) {
    material.maps[MATERIAL_MAP_NORMAL].value = 1.0f;
  }
  if (material.maps[MATERIAL_MAP_OCCLUSION].value <= 0.0f) {
    material.maps[MATERIAL_MAP_OCCLUSION].value = 1.0f;
  }
}

inline void apply_pbr_fallback_textures(rl::Material &material) {
  auto &fallbacks = pbr_fallback_textures();

  if (!has_texture(material.maps[MATERIAL_MAP_METALNESS].texture)) {
    material.maps[MATERIAL_MAP_METALNESS].texture = fallbacks.metallic_black;
  }
  if (!has_texture(material.maps[MATERIAL_MAP_ROUGHNESS].texture)) {
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = fallbacks.roughness_white;
  }
  if (!has_texture(material.maps[MATERIAL_MAP_NORMAL].texture)) {
    material.maps[MATERIAL_MAP_NORMAL].texture = fallbacks.normal_flat;
  }
  if (!has_texture(material.maps[MATERIAL_MAP_OCCLUSION].texture)) {
    material.maps[MATERIAL_MAP_OCCLUSION].texture = fallbacks.occlusion_white;
  }
  if (!has_texture(material.maps[MATERIAL_MAP_EMISSION].texture)) {
    material.maps[MATERIAL_MAP_EMISSION].texture = fallbacks.emission_black;
  }
}

inline void prepare_material(NonSendMarker marker, const MaterialKey &key,
                             const Assets<rl::Texture2D> &texture_assets,
                             PreparedMaterial &prepared) {
  const rl::Material &src_mat = fallback_material(marker);
  prepared.material = src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat.maps,
              sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();
  apply_pbr_defaults(prepared.material);

  if (key.shader) {
    prepared.material.shader = *key.shader;
  } else if (prepared.material.shader.id == 0 ||
             prepared.material.shader.locs == nullptr) {
    prepared.material.shader.id = rl::rlGetShaderIdDefault();
    prepared.material.shader.locs = rl::rlGetShaderLocsDefault();
  }

  auto multiply_color = [](rl::Color lhs, rl::Color rhs) -> rl::Color {
    return rl::Color{
        static_cast<u8>((lhs.r * rhs.r) / 255),
        static_cast<u8>((lhs.g * rhs.g) / 255),
        static_cast<u8>((lhs.b * rhs.b) / 255),
        static_cast<u8>((lhs.a * rhs.a) / 255),
    };
  };

  prepared.material.maps[MATERIAL_MAP_DIFFUSE].color = multiply_color(
      prepared.material.maps[MATERIAL_MAP_DIFFUSE].color, key.base_color);

  auto assign_texture = [&](int map_type, uint32_t texture_id) {
    if (texture_id == UINT32_MAX) {
      return;
    }
    if (auto *texture = texture_assets.get(Handle<rl::Texture2D>{texture_id})) {
      prepared.material.maps[map_type].texture = *texture;
    }
  };

  assign_texture(MATERIAL_MAP_DIFFUSE, key.texture_id);
  assign_texture(MATERIAL_MAP_NORMAL, key.normal_id);
  assign_texture(MATERIAL_MAP_METALNESS, key.metallic_id);
  assign_texture(MATERIAL_MAP_ROUGHNESS, key.roughness_id);
  assign_texture(MATERIAL_MAP_OCCLUSION, key.occlusion_id);
  assign_texture(MATERIAL_MAP_EMISSION, key.emissive_id);

  if (key.metallic >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_METALNESS].value = key.metallic;
  }
  if (key.roughness >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_ROUGHNESS].value = key.roughness;
  }
  if (key.normal_scale >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_NORMAL].value = key.normal_scale;
  }
  if (key.occlusion_strength >= 0.0f) {
    prepared.material.maps[MATERIAL_MAP_OCCLUSION].value =
        key.occlusion_strength;
  }
  if (key.emissive_color.a > 0) {
    prepared.material.maps[MATERIAL_MAP_EMISSION].color = key.emissive_color;
  }

  apply_pbr_fallback_textures(prepared.material);
}

inline void set_shader_value(NonSendMarker, rl::Shader &shader,
                             const char *name, const void *value, int type) {
  int loc = rl::GetShaderLocation(shader, name);
  if (loc >= 0) {
    rl::SetShaderValue(shader, loc, value, type);
  }
}

inline void set_shader_value_v(NonSendMarker, rl::Shader &shader,
                               const char *name, const void *value, int type,
                               int count) {
  int loc = rl::GetShaderLocation(shader, name);
  if (loc >= 0) {
    rl::SetShaderValueV(shader, loc, value, type, count);
  }
}

inline void set_shader_matrix(NonSendMarker, rl::Shader &shader,
                              const char *name, rl::Matrix value) {
  int loc = rl::GetShaderLocation(shader, name);
  if (loc >= 0) {
    rl::SetShaderValueMatrix(shader, loc, value);
  }
}

inline void bind_clustered_lighting(const ClusteredLightingState &state) {
  if (state.point_light_buffer != 0) {
    rl::rlBindShaderBuffer(state.point_light_buffer, 0);
  }
  if (state.cluster_buffer != 0) {
    rl::rlBindShaderBuffer(state.cluster_buffer, 1);
  }
  if (state.light_index_buffer != 0) {
    rl::rlBindShaderBuffer(state.light_index_buffer, 2);
  }
  if (state.overflow_buffer != 0) {
    rl::rlBindShaderBuffer(state.overflow_buffer, 3);
  }
}

inline void apply_shader_uniforms(NonSendMarker marker, rl::Shader &shader,
                                  const PreparedMaterial &prepared,
                                  const ExtractedView &view,
                                  const ExtractedLights &lights,
                                  const ClusterConfig &cluster_config,
                                  const ClusteredLightingState &cluster_state) {
  float metallic = prepared.material.maps[MATERIAL_MAP_METALNESS].value;
  float roughness = prepared.material.maps[MATERIAL_MAP_ROUGHNESS].value;
  float normal_factor = prepared.material.maps[MATERIAL_MAP_NORMAL].value;
  float occlusion_factor = prepared.material.maps[MATERIAL_MAP_OCCLUSION].value;
  rl::Vector3 emissive =
      color_to_vec3(prepared.material.maps[MATERIAL_MAP_EMISSION].color);
  rl::Vector4 tint = {1.0f, 1.0f, 1.0f, 0.0f};
  rl::Vector3 camera_pos = to_rl(view.camera_view.position);

  if (normal_factor <= 0.0f) {
    normal_factor = 1.0f;
  }
  if (occlusion_factor <= 0.0f) {
    occlusion_factor = 1.0f;
  }

  int has_directional = lights.has_directional ? 1 : 0;
  rl::Vector3 dir_color = color_to_vec3(lights.dir_light.color);
  rl::Vector3 dir_dir = to_rl(lights.dir_light.direction.Normalized());
  float dir_intensity = lights.dir_light.intensity;
  rl::Matrix view_matrix = to_rl(view.camera_view.view);
  rl::Vector2 cluster_screen_size = {
      static_cast<float>(std::max(rl::GetScreenWidth(), 1)),
      static_cast<float>(std::max(rl::GetScreenHeight(), 1)),
  };
  rl::Vector2 cluster_near_far = {
      std::max(view.near_plane, 0.001f),
      std::max(view.far_plane, view.near_plane + 0.001f),
  };
  int cluster_dimensions[4] = {cluster_config.tile_count_x,
                               cluster_config.tile_count_y,
                               cluster_config.slice_count_z,
                               0};
  int cluster_max_lights = static_cast<int>(cluster_config.max_lights_per_cluster);
  int is_orthographic = view.orthographic ? 1 : 0;
  float alpha_cutoff = 0.1f;

  bind_clustered_lighting(cluster_state);

  set_shader_value(marker, shader, "u_metallic_factor", &metallic,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(marker, shader, "u_roughness_factor", &roughness,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(marker, shader, "u_normal_factor", &normal_factor,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(marker, shader, "u_occlusion_factor", &occlusion_factor,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(marker, shader, "u_emissive_factor", &emissive,
                   SHADER_UNIFORM_VEC3);
  set_shader_value(marker, shader, "u_color", &tint, SHADER_UNIFORM_VEC4);
  set_shader_value(marker, shader, "u_camera_pos", &camera_pos,
                   SHADER_UNIFORM_VEC3);

  set_shader_value(marker, shader, "u_has_directional", &has_directional,
                   SHADER_UNIFORM_INT);
  set_shader_value(marker, shader, "u_dir_light_direction", &dir_dir,
                   SHADER_UNIFORM_VEC3);
  set_shader_value(marker, shader, "u_dir_light_intensity", &dir_intensity,
                   SHADER_UNIFORM_FLOAT);
  set_shader_value(marker, shader, "u_dir_light_color", &dir_color,
                   SHADER_UNIFORM_VEC3);
  set_shader_matrix(marker, shader, "u_view", view_matrix);
  set_shader_value(marker, shader, "u_cluster_dimensions", cluster_dimensions,
                   SHADER_UNIFORM_IVEC4);
  set_shader_value(marker, shader, "u_cluster_screen_size",
                   &cluster_screen_size, SHADER_UNIFORM_VEC2);
  set_shader_value(marker, shader, "u_cluster_near_far", &cluster_near_far,
                   SHADER_UNIFORM_VEC2);
  set_shader_value(marker, shader, "u_cluster_max_lights",
                   &cluster_max_lights, SHADER_UNIFORM_INT);
  set_shader_value(marker, shader, "u_is_orthographic", &is_orthographic,
                   SHADER_UNIFORM_INT);
  set_shader_value(marker, shader, "u_alpha_cutoff", &alpha_cutoff,
                   SHADER_UNIFORM_FLOAT);
}

inline bool can_draw_instanced(rl::Material &material, const rl::Mesh &mesh) {
  if (!material.shader.locs || mesh.vaoId == 0) {
    return false;
  }

  if (material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] < 0) {
    material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
        rl::GetShaderLocationAttrib(material.shader, "instanceTransform");
  }
  if (material.shader.locs[SHADER_LOC_MATRIX_MODEL] < 0) {
    material.shader.locs[SHADER_LOC_MATRIX_MODEL] =
        rl::GetShaderLocation(material.shader, "matModel");
  }

  return material.shader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] >= 0;
}

inline void draw_batch_instances(const rl::Mesh &mesh, rl::Material &material,
                                 const OpaqueBatch &batch) {
  if (can_draw_instanced(material, mesh)) {
    rl::DrawMeshInstanced(mesh, material, batch.transforms.data(),
                          static_cast<i32>(batch.transforms.size()));
    return;
  }

  for (const auto &transform : batch.transforms) {
    rl::DrawMesh(mesh, material, transform);
  }
}

} // namespace ecs
