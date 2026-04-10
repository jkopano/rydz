#pragma once

#include "clustered_lighting.hpp"
#include "render_batches.hpp"
#include "render_extract.hpp"
#include "rydz_gl/resources.hpp"
#include "rydz_gl/state.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace ecs {

struct PreparedMaterial {
  std::array<rydz_gl::MaterialMap, 12> local_maps{};
  rydz_gl::Material material{};
};

struct PbrFallbackTextures {
  rydz_gl::Texture metallic_black{};
  rydz_gl::Texture roughness_white{};
  rydz_gl::Texture normal_flat{};
  rydz_gl::Texture occlusion_white{};
  rydz_gl::Texture emission_black{};
};

struct ShaderCache {
  using T = Resource;
  std::unordered_map<ShaderSpec, ShaderProgram> shaders;
};

inline rydz_gl::Material &fallback_material(NonSendMarker) {
  static rydz_gl::Material fallback = {};
  static bool init = false;
  if (!init) {
    fallback.shader.id = rydz_gl::default_shader_id();
    fallback.shader.locs = rydz_gl::default_shader_locs();
    fallback.maps =
        static_cast<rydz_gl::MaterialMap *>(std::calloc(12, sizeof(rydz_gl::MaterialMap)));
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].texture.id =
        rydz_gl::default_texture_id();
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].texture.width = 1;
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].texture.height = 1;
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].texture.mipmaps = 1;
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].texture.format =
        rydz_gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    fallback.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].color = rydz_gl::kWhite;
    init = true;
  }
  return fallback;
}

inline PbrFallbackTextures &pbr_fallback_textures() {
  static PbrFallbackTextures textures = [] {
    auto make_texture = [](rydz_gl::Color color) {
      rydz_gl::Image image = rydz_gl::gen_image_color(1, 1, color);
      rydz_gl::Texture texture = rydz_gl::load_texture_from_image(image);
      rydz_gl::unload_image(image);
      return texture;
    };

    return PbrFallbackTextures{
        .metallic_black = make_texture({0, 0, 0, 255}),
        .roughness_white = make_texture(rydz_gl::kWhite),
        .normal_flat = make_texture({128, 128, 255, 255}),
        .occlusion_white = make_texture(rydz_gl::kWhite),
        .emission_black = make_texture({0, 0, 0, 255}),
    };
  }();
  return textures;
}

inline void apply_pbr_defaults(rydz_gl::Material &material) {
  if (material.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].color.a == 0) {
    material.maps[rydz_gl::MATERIAL_MAP_DIFFUSE].color = rydz_gl::kWhite;
  }
  if (material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].value <= 0.0f &&
      !rydz_gl::has_texture(
          material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].value = 1.0f;
  }
  if (material.maps[rydz_gl::MATERIAL_MAP_NORMAL].value <= 0.0f) {
    material.maps[rydz_gl::MATERIAL_MAP_NORMAL].value = 1.0f;
  }
  if (material.maps[rydz_gl::MATERIAL_MAP_OCCLUSION].value <= 0.0f) {
    material.maps[rydz_gl::MATERIAL_MAP_OCCLUSION].value = 1.0f;
  }
}

inline void apply_pbr_fallback_textures(rydz_gl::Material &material) {
  auto &fallbacks = pbr_fallback_textures();

  if (!rydz_gl::has_texture(
          material.maps[rydz_gl::MATERIAL_MAP_METALNESS].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_METALNESS].texture =
        fallbacks.metallic_black;
  }
  if (!rydz_gl::has_texture(
          material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].texture =
        fallbacks.roughness_white;
  }
  if (!rydz_gl::has_texture(material.maps[rydz_gl::MATERIAL_MAP_NORMAL].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_NORMAL].texture = fallbacks.normal_flat;
  }
  if (!rydz_gl::has_texture(
          material.maps[rydz_gl::MATERIAL_MAP_OCCLUSION].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_OCCLUSION].texture =
        fallbacks.occlusion_white;
  }
  if (!rydz_gl::has_texture(
          material.maps[rydz_gl::MATERIAL_MAP_EMISSION].texture)) {
    material.maps[rydz_gl::MATERIAL_MAP_EMISSION].texture =
        fallbacks.emission_black;
  }
}

inline ShaderProgram &resolve_shader(NonSendMarker, ShaderCache &cache,
                                     const ShaderSpec &spec) {
  auto [it, inserted] = cache.shaders.try_emplace(spec);
  if (inserted) {
    it->second = ShaderProgram::load(spec);
  }
  return it->second;
}

inline void initialize_custom_map_locations(ShaderProgram &shader) {
  auto &raw = shader.raw();
  if (raw.locs == nullptr) {
    return;
  }

  if (raw.locs[rydz_gl::SHADER_LOC_MAP_ROUGHNESS] < 0) {
    raw.locs[rydz_gl::SHADER_LOC_MAP_ROUGHNESS] =
        rydz_gl::shader_location(raw, "u_roughness_texture");
  }
  if (raw.locs[rydz_gl::SHADER_LOC_MAP_OCCLUSION] < 0) {
    raw.locs[rydz_gl::SHADER_LOC_MAP_OCCLUSION] =
        rydz_gl::shader_location(raw, "u_occlusion_texture");
  }
  if (raw.locs[rydz_gl::SHADER_LOC_MAP_EMISSION] < 0) {
    raw.locs[rydz_gl::SHADER_LOC_MAP_EMISSION] =
        rydz_gl::shader_location(raw, "u_emissive_texture");
  }
}

inline void apply_material_map_binding(rydz_gl::Material &material,
                                       const MaterialMapBinding &binding,
                                       const Assets<Texture> &textures) {
  if (binding.map_type < 0 || binding.map_type >= 12) {
    return;
  }

  auto &map = material.maps[binding.map_type];
  if (binding.has_color) {
    map.color = binding.color;
  }
  if (binding.has_value) {
    map.value = binding.value;
  }
  if (binding.has_texture) {
    if (const auto *texture = textures.get(binding.texture)) {
      map.texture = *texture;
    }
  }
}

inline void prepare_material(NonSendMarker marker,
                             const MaterialDescriptor &descriptor,
                             const Assets<Texture> &texture_assets,
                             ShaderCache &shader_cache,
                             PreparedMaterial &prepared) {
  const auto &src_mat = fallback_material(marker);
  prepared.material = src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat.maps,
              sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();

  ShaderProgram &shader = resolve_shader(marker, shader_cache, descriptor.shader);
  initialize_custom_map_locations(shader);
  prepared.material.shader = shader.raw();

  for (const auto &binding : descriptor.maps) {
    apply_material_map_binding(prepared.material, binding, texture_assets);
  }

  if (descriptor.shading_model == MaterialShadingModel::ClusteredPbr) {
    apply_pbr_defaults(prepared.material);
    apply_pbr_fallback_textures(prepared.material);
  }
}

inline void apply_pbr_shader_uniforms(
    NonSendMarker, ShaderProgram &shader, const PreparedMaterial &prepared,
    const ExtractedView &view, const ExtractedLights &lights,
    const ClusterConfig &cluster_config,
    const ClusteredLightingState &cluster_state) {
  float metallic =
      prepared.material.maps[rydz_gl::MATERIAL_MAP_METALNESS].value;
  float roughness =
      prepared.material.maps[rydz_gl::MATERIAL_MAP_ROUGHNESS].value;
  float normal_factor =
      prepared.material.maps[rydz_gl::MATERIAL_MAP_NORMAL].value;
  float occlusion_factor =
      prepared.material.maps[rydz_gl::MATERIAL_MAP_OCCLUSION].value;
  rydz_gl::Vec3 emissive = color_to_vec3(
      prepared.material.maps[rydz_gl::MATERIAL_MAP_EMISSION].color);
  rydz_gl::Vec4 tint = {1.0f, 1.0f, 1.0f, 0.0f};
  rydz_gl::Vec3 camera_pos = rydz_gl::to_vector3(view.camera_view.position);

  if (normal_factor <= 0.0f) {
    normal_factor = 1.0f;
  }
  if (occlusion_factor <= 0.0f) {
    occlusion_factor = 1.0f;
  }

  int has_directional = lights.has_directional ? 1 : 0;
  rydz_gl::Vec3 dir_color = color_to_vec3(lights.dir_light.color);
  rydz_gl::Vec3 dir_dir =
      rydz_gl::to_vector3(lights.dir_light.direction.Normalized());
  float dir_intensity = lights.dir_light.intensity;
  rydz_gl::Matrix view_matrix = rydz_gl::to_matrix(view.camera_view.view);
  rydz_gl::Vec2 cluster_screen_size = {
      static_cast<float>(std::max(rydz_gl::screen_width(), 1)),
      static_cast<float>(std::max(rydz_gl::screen_height(), 1)),
  };
  rydz_gl::Vec2 cluster_near_far = {
      std::max(view.near_plane, 0.001f),
      std::max(view.far_plane, view.near_plane + 0.001f),
  };
  int cluster_dimensions[4] = {cluster_config.tile_count_x,
                               cluster_config.tile_count_y,
                               cluster_config.slice_count_z, 0};
  int cluster_max_lights =
      static_cast<int>(cluster_config.max_lights_per_cluster);
  int is_orthographic = view.orthographic ? 1 : 0;
  float alpha_cutoff = 0.1f;

  rydz_gl::bind_clustered_lighting(cluster_state);

  shader.set("u_metallic_factor", metallic);
  shader.set("u_roughness_factor", roughness);
  shader.set("u_normal_factor", normal_factor);
  shader.set("u_occlusion_factor", occlusion_factor);
  shader.set("u_emissive_factor", emissive);
  shader.set("u_color", tint);
  shader.set("u_camera_pos", camera_pos);

  shader.set("u_has_directional", has_directional);
  shader.set("u_dir_light_direction", dir_dir);
  shader.set("u_dir_light_intensity", dir_intensity);
  shader.set("u_dir_light_color", dir_color);
  shader.set("matView", view_matrix);
  shader.set_ints("u_cluster_dimensions", cluster_dimensions,
                  rydz_gl::SHADER_UNIFORM_IVEC4);
  shader.set("u_cluster_screen_size", cluster_screen_size);
  shader.set("u_cluster_near_far", cluster_near_far);
  shader.set("u_cluster_max_lights", cluster_max_lights);
  shader.set("u_is_orthographic", is_orthographic);
  shader.set("u_alpha_cutoff", alpha_cutoff);
}

inline void apply_descriptor_uniforms(NonSendMarker, ShaderProgram &shader,
                                      const MaterialDescriptor &descriptor) {
  for (const auto &uniform : descriptor.uniforms) {
    shader.apply(uniform);
  }
}

inline void apply_shader_uniforms(
    NonSendMarker marker, ShaderProgram &shader,
    const MaterialDescriptor &descriptor, const PreparedMaterial &prepared,
    const ExtractedView &view, const ExtractedLights &lights,
    const ClusterConfig &cluster_config,
    const ClusteredLightingState &cluster_state) {
  shader.set("u_alpha_cutoff", 0.1f);
  shader.set("u_use_instancing", 0);

  if (descriptor.shading_model == MaterialShadingModel::ClusteredPbr) {
    apply_pbr_shader_uniforms(marker, shader, prepared, view, lights,
                              cluster_config, cluster_state);
  }

  apply_descriptor_uniforms(marker, shader, descriptor);
}

inline bool can_draw_instanced(rydz_gl::Material &material,
                               const rydz_gl::Mesh &mesh) {
  if (!material.shader.locs || mesh.vaoId == 0) {
    return false;
  }

  if (material.shader.locs[rydz_gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] < 0) {
    material.shader.locs[rydz_gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
        rydz_gl::shader_location_attrib(material.shader, "instanceTransform");
  }
  if (material.shader.locs[rydz_gl::SHADER_LOC_MATRIX_MODEL] < 0) {
    material.shader.locs[rydz_gl::SHADER_LOC_MATRIX_MODEL] =
        rydz_gl::shader_location(material.shader, "matModel");
  }

  return material.shader.locs[rydz_gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] >=
         0;
}

inline void draw_batch_instances(ShaderProgram &shader,
                                 const rydz_gl::Mesh &mesh,
                                 rydz_gl::Material &material,
                                 const OpaqueBatch &batch) {
  if (can_draw_instanced(material, mesh)) {
    shader.set("u_use_instancing", 1);
    rydz_gl::draw_mesh_instanced(
        mesh, material, batch.transforms.data(),
        static_cast<i32>(batch.transforms.size()));
    return;
  }

  shader.set("u_use_instancing", 0);
  for (const auto &transform : batch.transforms) {
    rydz_gl::draw_mesh(mesh, material, transform);
  }
}

inline void draw_single_instance(ShaderProgram &shader,
                                 const rydz_gl::Mesh &mesh,
                                 rydz_gl::Material &material,
                                 const rydz_gl::Matrix &transform) {
  if (can_draw_instanced(material, mesh)) {
    shader.set("u_use_instancing", 1);
    rydz_gl::draw_mesh_instanced(mesh, material, &transform, 1);
    return;
  }

  shader.set("u_use_instancing", 0);
  rydz_gl::draw_mesh(mesh, material, transform);
}

} // namespace ecs
