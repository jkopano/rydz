#pragma once

#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/material/slot_provider.hpp"
#include "rydz_graphics/pipeline/graph.hpp"
#include "rydz_graphics/pipeline/pass_context.hpp"
#include "rydz_graphics/pipeline/batches.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ecs {

struct PbrFallbackTextures {
  gl::Texture metallic_black;
  gl::Texture roughness_white;
  gl::Texture normal_flat;
  gl::Texture occlusion_white;
  gl::Texture emission_black;
};

struct ShaderCache {
  using T = Resource;
  std::unordered_map<ShaderSpec, ShaderProgram> shaders;
};

inline auto MaterialContext::frame() const -> PassContext const& {
  return *frame_data;
}

inline auto MaterialContext::textures() const -> Assets<Texture> const& {
  return frame().texture_assets;
}

inline auto MaterialContext::view() const -> ExtractedView const& {
  return frame().view;
}

inline auto MaterialContext::lights() const -> ExtractedLights const& {
  return frame().lights;
}

inline auto MaterialContext::cluster_config() const -> ClusterConfig const& {
  return frame().cluster_config;
}

inline auto MaterialContext::clustered_lighting() const
  -> ClusteredLightingState const& {
  return frame().cluster_state;
}

inline auto MaterialContext::time() const -> Time const& {
  return frame().time;
}

inline auto MaterialContext::slots() const -> SlotProviderRegistry const& {
  return frame().slot_registry;
}

inline auto pbr_fallback_textures() -> PbrFallbackTextures& {
  static PbrFallbackTextures textures = [] -> PbrFallbackTextures {
    auto make_texture = [](Color color) -> gl::Texture {
      gl::Image image = gl::gen_image_with_color(1, 1, color);
      gl::Texture texture = image.load_texture();
      gl::unload_image(image);
      return texture;
    };

    return PbrFallbackTextures{
      .metallic_black = make_texture(Color::BLACK),
      .roughness_white = make_texture(Color::WHITE),
      .normal_flat = make_texture({128, 128, 255, 255}),
      .occlusion_white = make_texture(Color::WHITE),
      .emission_black = make_texture(Color::BLACK),
    };
  }();
  return textures;
}

inline auto fallback_texture(MaterialMap map) -> gl::Texture const* {
  auto& fallbacks = pbr_fallback_textures();
  switch (map) {
  case MaterialMap::Metalness:
    return &fallbacks.metallic_black;
  case MaterialMap::Normal:
    return &fallbacks.normal_flat;
  case MaterialMap::Roughness:
    return &fallbacks.roughness_white;
  case MaterialMap::Occlusion:
    return &fallbacks.occlusion_white;
  case MaterialMap::Emission:
    return &fallbacks.emission_black;
  default:
    return nullptr;
  }
}

inline auto apply_default_material_map_textures(gl::Material& material) -> void {
  auto* maps = material.maps;
  for (auto const map : DEFAULT_MATERIAL_MAPS) {
    auto& material_map = maps[material_map_index(map)];
    if (material_map.texture.ready()) {
      continue;
    }

    auto const* fallback = fallback_texture(map);
    if (fallback != nullptr) {
      material_map.texture = *fallback;
    }
  }
}

inline auto resolve_shader(NonSendMarker, ShaderCache& cache, ShaderSpec const& spec)
  -> ShaderProgram& {
  auto [it, inserted] = cache.shaders.try_emplace(spec);
  if (inserted) {
    it->second = ShaderProgram::load(spec);
  }
  return it->second;
}

inline auto initialize_material_map_location(ShaderProgram& shader, MaterialMap map)
  -> void {
  auto& raw = shader.raw();
  if (raw.locs == nullptr) {
    return;
  }

  int const location_index = shader_location_index(map);
  if (raw.locs[location_index] < 0) {
    raw.locs[location_index] = shader.uniform_location(map_texture_binding(map));
  }
}

inline auto initialize_material_map_locations(ShaderProgram& shader) -> void {
  initialize_material_map_location(shader, MaterialMap::Albedo);
  initialize_material_map_location(shader, MaterialMap::Metalness);
  initialize_material_map_location(shader, MaterialMap::Normal);
  initialize_material_map_location(shader, MaterialMap::Roughness);
  initialize_material_map_location(shader, MaterialMap::Occlusion);
  initialize_material_map_location(shader, MaterialMap::Emission);
}

inline auto apply_material_map_binding(
  gl::Material& material,
  MaterialMapBinding const& binding,
  Assets<Texture> const& textures
) -> void {
  int const map_index = material_map_index(binding.map_type);
  if (map_index < 0 || map_index >= K_MATERIAL_MAP_COUNT) {
    return;
  }

  auto& map = material.maps[map_index];
  if (binding.has_color) {
    map.color = binding.color;
  }
  if (binding.has_value) {
    map.value = binding.value;
  }
  if (binding.has_texture) {
    if (auto const* texture = textures.get(binding.texture)) {
      map.texture = *texture;
    }
  }
}

inline auto lookup_slot_provider(
  SlotProviderRegistry const& registry,
  MaterialSlotRequirement const& slot,
  CompiledMaterial const& material
) -> SlotProvider const& {
  auto iter = registry.providers.find(slot.type);
  if (iter != registry.providers.end()) {
    return iter->second;
  }

  throw std::runtime_error(
    "Missing slot provider for '" + slot.debug_name + "' required by material '" +
    material.material_type_name + "' (" + material.shader.vertex_path + ", " +
    material.shader.fragment_path + ")"
  );
}

inline auto prepare_material(
  MaterialContext const& material_ctx,
  CompiledMaterial const& material,
  ShaderSpec const& shader_spec,
  PreparedMaterial& prepared
) -> ShaderProgram& {
  auto const& src_mat = gl::Material::fallback_material(material_ctx.frame().marker);
  prepared.material = src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat.maps, sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();

  ShaderProgram& shader = resolve_shader(
    material_ctx.frame().marker, material_ctx.frame().shader_cache, shader_spec
  );
  initialize_material_map_locations(shader);
  prepared.material.shader = shader.raw();

  for (auto const& binding : material.maps) {
    apply_material_map_binding(prepared.material, binding, material_ctx.textures());
  }
  apply_default_material_map_textures(prepared.material);

  for (auto const& slot : material.slots) {
    auto const& provider = lookup_slot_provider(material_ctx.slots(), slot, material);
    if (provider.prepare) {
      provider.prepare(material_ctx, material, prepared, shader);
    }
  }

  return shader;
}

inline auto apply_slot_uniforms_per_view(
  MaterialContext const& material_ctx,
  CompiledMaterial const& material,
  ShaderProgram& shader
) -> void {
  for (auto const& slot : material.slots) {
    auto const& provider = lookup_slot_provider(material_ctx.slots(), slot, material);
    if (provider.apply_per_view) {
      provider.apply_per_view(material_ctx, shader);
    }
  }
}

inline auto apply_slot_uniforms_per_material(
  MaterialContext const& material_ctx,
  CompiledMaterial const& material,
  PreparedMaterial const& prepared,
  ShaderProgram& shader
) -> void {
  for (auto const& slot : material.slots) {
    auto const& provider = lookup_slot_provider(material_ctx.slots(), slot, material);
    if (provider.apply_per_material) {
      provider.apply_per_material(material_ctx, material, prepared, shader);
    }
  }
}

inline auto HasCamera::slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.apply_per_view =
    [](MaterialContext const& ctx, ShaderProgram& shader) -> void {
    shader.set(CameraUniform::Position, ctx.view().camera_view.position);
    shader.set(CameraUniform::ViewMatrix, ctx.view().camera_view.view);
    shader.set(CameraUniform::ProjectionMatrix, ctx.view().camera_view.proj);
  };
  return provider;
}

inline auto HasTime::slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.apply_per_view =
    [](MaterialContext const& ctx, ShaderProgram& shader) -> void {
    shader.set("u_time", ctx.time().elapsed_seconds);
  };
  return provider;
}

inline auto HasPBR::slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.apply_per_view =
    [](MaterialContext const& ctx, ShaderProgram& shader) -> void {
    auto const& view = ctx.view();
    auto const& lights = ctx.lights();
    auto const& cluster_config = ctx.cluster_config();

    int has_directional = lights.has_directional ? 1 : 0;
    Vec3 dir_color = lights.dir_light.color;
    Vec3 dir_dir = lights.dir_light.direction.normalized();
    float dir_intensity = lights.dir_light.intensity;
    Vec2 cluster_screen_size = {
      std::max(view.viewport.width, 1.0F),
      std::max(view.viewport.height, 1.0F),
    };
    Vec2 cluster_near_far = {
      std::max(view.near_plane, 0.001f),
      std::max(view.far_plane, view.near_plane + 0.001f),
    };
    std::array<int, 4> cluster_dimensions = {
      cluster_config.tile_count_x_clamped(),
      cluster_config.tile_count_y_clamped(),
      cluster_config.slice_count_z_clamped(),
      0
    };
    int cluster_max_lights =
      static_cast<int>(cluster_config.max_lights_per_cluster_clamped());
    int is_orthographic = view.orthographic ? 1 : 0;

    ctx.clustered_lighting().bind();
    shader.set(PbrLightingUniform::HasDirectional, has_directional);
    shader.set(PbrLightingUniform::DirectionalDirection, dir_dir);
    shader.set(PbrLightingUniform::DirectionalIntensity, dir_intensity);
    shader.set(PbrLightingUniform::DirectionalColor, dir_color);
    shader.set(PbrLightingUniform::ClusterDimensions, cluster_dimensions);
    shader.set(PbrLightingUniform::ClusterScreenSize, cluster_screen_size);
    shader.set(PbrLightingUniform::ClusterNearFar, cluster_near_far);
    shader.set(PbrLightingUniform::ClusterMaxLights, cluster_max_lights);
    shader.set(PbrLightingUniform::IsOrthographic, is_orthographic);
  };
  return provider;
}

template <typename BatchT>
inline auto draw_batch(
  ShaderProgram& shader, gl::Mesh const& mesh, gl::Material& material, BatchT const& batch
) -> void {
  (void) shader;
  mesh.draw_instanced(
    material, batch.transforms.data(), static_cast<i32>(batch.transforms.size())
  );
}

} // namespace ecs
