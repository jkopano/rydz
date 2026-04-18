#pragma once

#include "rydz_graphics/clustered_lighting.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/render_batches.hpp"
#include "rydz_graphics/render_extract.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace ecs {

struct PreparedMaterial {
  std::array<gl::MaterialMap, K_MATERIAL_MAP_COUNT> local_maps{};
  gl::Material material;
};

struct SlotPrepareContext {
  Assets<Texture> const* texture_assets = nullptr;
  bool instanced = false;

  [[nodiscard]] auto textures() const -> Assets<Texture> const& {
    return *texture_assets;
  }
};

struct RenderSlotContext {
  ExtractedView const* view_data = nullptr;
  ExtractedLights const* lights_data = nullptr;
  ClusterConfig const* cluster_config_data = nullptr;
  ClusteredLightingState const* cluster_state_data = nullptr;
  bool instanced = false;

  [[nodiscard]] auto view() const -> ExtractedView const& { return *view_data; }
  [[nodiscard]] auto lights() const -> ExtractedLights const& {
    return *lights_data;
  }
  [[nodiscard]] auto cluster_config() const -> ClusterConfig const& {
    return *cluster_config_data;
  }
  [[nodiscard]] auto clustered_lighting() const
    -> ClusteredLightingState const& {
    return *cluster_state_data;
  }
};

struct SlotProvider {
  std::function<void(
    SlotPrepareContext const&,
    CompiledMaterial const&,
    PreparedMaterial&,
    ShaderProgram&
  )>
    prepare;
  std::function<void(
    RenderSlotContext const&,
    CompiledMaterial const&,
    PreparedMaterial const&,
    ShaderProgram&
  )>
    apply;
};

struct SlotProviderRegistry {
  using T = Resource;
  std::unordered_map<std::type_index, SlotProvider> providers;

  template <typename SlotT> void register_slot(SlotProvider provider) {
    this->providers[typeid(SlotT)] = std::move(provider);
  }
};

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

inline auto apply_pbr_defaults(gl::Material& material) -> void {
  auto* maps = material.maps;
  if (maps[material_map_index(MaterialMap::Albedo)].color.a == 0) {
    maps[material_map_index(MaterialMap::Albedo)].color = Color::WHITE;
  }
  if (maps[material_map_index(MaterialMap::Roughness)].value <= 0.0F &&
      !maps[material_map_index(MaterialMap::Roughness)].texture.ready()) {
    maps[material_map_index(MaterialMap::Roughness)].value = 1.0F;
  }
  if (maps[material_map_index(MaterialMap::Normal)].value <= 0.0f) {
    maps[material_map_index(MaterialMap::Normal)].value = 1.0f;
  }
  if (maps[material_map_index(MaterialMap::Occlusion)].value <= 0.0f) {
    maps[material_map_index(MaterialMap::Occlusion)].value = 1.0f;
  }
}

inline auto apply_pbr_fallback_textures(gl::Material& material) -> void {
  auto& fallbacks = pbr_fallback_textures();

  auto* maps = material.maps;
  if (!maps[material_map_index(MaterialMap::Metalness)].texture.ready()) {
    maps[material_map_index(MaterialMap::Metalness)].texture =
      fallbacks.metallic_black;
  }
  if (!maps[material_map_index(MaterialMap::Roughness)].texture.ready()) {
    maps[material_map_index(MaterialMap::Roughness)].texture =
      fallbacks.roughness_white;
  }
  if (!maps[material_map_index(MaterialMap::Normal)].texture.ready()) {
    maps[material_map_index(MaterialMap::Normal)].texture =
      fallbacks.normal_flat;
  }
  if (!maps[material_map_index(MaterialMap::Occlusion)].texture.ready()) {
    maps[material_map_index(MaterialMap::Occlusion)].texture =
      fallbacks.occlusion_white;
  }
  if (!maps[material_map_index(MaterialMap::Emission)].texture.ready()) {
    maps[material_map_index(MaterialMap::Emission)].texture =
      fallbacks.emission_black;
  }
}

inline auto resolve_shader(
  NonSendMarker, ShaderCache& cache, ShaderSpec const& spec
) -> ShaderProgram& {
  auto [it, inserted] = cache.shaders.try_emplace(spec);
  if (inserted) {
    it->second = ShaderProgram::load(spec);
  }
  return it->second;
}

inline auto initialize_material_map_location(
  ShaderProgram& shader, MaterialMap map
) -> void {
  auto& raw = shader.raw();
  if (raw.locs == nullptr) {
    return;
  }

  int const location_index = shader_location_index(map);
  if (raw.locs[location_index] < 0) {
    raw.locs[location_index] =
      shader.uniform_location(map_texture_binding(map));
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
    "Missing slot provider for '" + slot.debug_name +
    "' required by material '" + material.material_type_name + "' (" +
    material.shader.vertex_path + ", " + material.shader.fragment_path + ")"
  );
}

inline auto prepare_material(
  NonSendMarker marker,
  CompiledMaterial const& material,
  Assets<Texture> const& texture_assets,
  ShaderCache& shader_cache,
  SlotProviderRegistry const& slot_registry,
  SlotPrepareContext const& prepare_ctx,
  ShaderSpec const& shader_spec,
  PreparedMaterial& prepared
) -> ShaderProgram& {
  auto const& src_mat = gl::Material::fallback_material(marker);
  prepared.material = src_mat;
  std::memcpy(
    prepared.local_maps.data(), src_mat.maps, sizeof(prepared.local_maps)
  );
  prepared.material.maps = prepared.local_maps.data();

  ShaderProgram& shader = resolve_shader(marker, shader_cache, shader_spec);
  initialize_material_map_locations(shader);
  prepared.material.shader = shader.raw();

  for (auto const& binding : material.maps) {
    apply_material_map_binding(prepared.material, binding, texture_assets);
  }

  for (auto const& slot : material.slots) {
    auto const& provider = lookup_slot_provider(slot_registry, slot, material);
    if (provider.prepare) {
      provider.prepare(prepare_ctx, material, prepared, shader);
    }
  }

  return shader;
}

inline auto apply_slot_uniforms(
  SlotProviderRegistry const& slot_registry,
  RenderSlotContext const& render_ctx,
  CompiledMaterial const& material,
  PreparedMaterial const& prepared,
  ShaderProgram& shader
) -> void {
  for (auto const& slot : material.slots) {
    auto const& provider = lookup_slot_provider(slot_registry, slot, material);
    if (provider.apply) {
      provider.apply(render_ctx, material, prepared, shader);
    }
  }
}

inline auto make_has_camera_slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.apply = [](
                     RenderSlotContext const& ctx,
                     CompiledMaterial const&,
                     PreparedMaterial const&,
                     ShaderProgram& shader
                   ) -> void {
    shader.set(
      CameraUniform::Position, math::to_rl(ctx.view().camera_view.position)
    );
    shader.set(CameraUniform::ViewMatrix, ctx.view().camera_view.view);
    shader.set(CameraUniform::ProjectionMatrix, ctx.view().camera_view.proj);
  };
  return provider;
}

inline auto make_has_pbr_slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.prepare = [](
                       SlotPrepareContext const&,
                       CompiledMaterial const&,
                       PreparedMaterial& prepared,
                       ShaderProgram&
                     ) -> void {
    apply_pbr_defaults(prepared.material);
    apply_pbr_fallback_textures(prepared.material);
  };
  provider.apply = [](
                     RenderSlotContext const& ctx,
                     CompiledMaterial const&,
                     PreparedMaterial const&,
                     ShaderProgram& shader
                   ) -> void {
    auto const& view = ctx.view();
    auto const& lights = ctx.lights();
    auto const& cluster_config = ctx.cluster_config();

    int has_directional = lights.has_directional ? 1 : 0;
    gl::Vec3 dir_color = lights.dir_light.color;
    gl::Vec3 dir_dir = math::to_rl(lights.dir_light.direction.normalized());
    float dir_intensity = lights.dir_light.intensity;
    gl::Vec2 cluster_screen_size = {
      std::max(view.viewport.width, 1.0F),
      std::max(view.viewport.height, 1.0F),
    };
    gl::Vec2 cluster_near_far = {
      std::max(view.near_plane, 0.001f),
      std::max(view.far_plane, view.near_plane + 0.001f),
    };
    std::array<int, 4> cluster_dimensions = {
      cluster_config.tile_count_x,
      cluster_config.tile_count_y,
      cluster_config.slice_count_z,
      0
    };
    int cluster_max_lights =
      static_cast<int>(cluster_config.max_lights_per_cluster);
    int is_orthographic = view.orthographic ? 1 : 0;

    gl::bind_clustered_lighting(ctx.clustered_lighting());
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
  ShaderProgram& shader,
  gl::Mesh const& mesh,
  gl::Material& material,
  BatchT const& batch
) -> void {
  (void) shader;
  mesh.draw_instanced(
    material, batch.transforms.data(), static_cast<i32>(batch.transforms.size())
  );
}

} // namespace ecs
