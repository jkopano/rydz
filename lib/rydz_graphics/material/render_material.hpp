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
  std::array<gl::MaterialMap, kMaterialMapCount> local_maps{};
  gl::Material material;
};

struct SlotPrepareContext {
  const Assets<Texture> *texture_assets = nullptr;
  bool instanced = false;

  [[nodiscard]] auto textures() const -> const Assets<Texture> & {
    return *texture_assets;
  }
};

struct RenderSlotContext {
  const ExtractedView *view_data = nullptr;
  const ExtractedLights *lights_data = nullptr;
  const ClusterConfig *cluster_config_data = nullptr;
  const ClusteredLightingState *cluster_state_data = nullptr;
  bool instanced = false;

  [[nodiscard]] auto view() const -> const ExtractedView & {
    return *view_data;
  }
  [[nodiscard]] auto lights() const -> const ExtractedLights & {
    return *lights_data;
  }
  [[nodiscard]] auto cluster_config() const -> const ClusterConfig & {
    return *cluster_config_data;
  }
  [[nodiscard]] auto clustered_lighting() const
      -> const ClusteredLightingState & {
    return *cluster_state_data;
  }
};

struct SlotProvider {
  std::function<void(const SlotPrepareContext &, const CompiledMaterial &,
                     PreparedMaterial &, ShaderProgram &)>
      prepare;
  std::function<void(const RenderSlotContext &, const CompiledMaterial &,
                     const PreparedMaterial &, ShaderProgram &)>
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

inline auto pbr_fallback_textures() -> PbrFallbackTextures & {
  static PbrFallbackTextures textures = [] -> PbrFallbackTextures {
    auto make_texture = [](gl::Color color) -> gl::Texture {
      gl::Image image = gl::gen_image_with_color(1, 1, color);
      gl::Texture texture = image.load_texture();
      gl::unload_image(image);
      return texture;
    };

    return PbrFallbackTextures{
        .metallic_black = make_texture({0, 0, 0, 255}),
        .roughness_white = make_texture(gl::kWhite),
        .normal_flat = make_texture({128, 128, 255, 255}),
        .occlusion_white = make_texture(gl::kWhite),
        .emission_black = make_texture({0, 0, 0, 255}),
    };
  }();
  return textures;
}

inline auto apply_pbr_defaults(gl::Material &material) -> void {
  auto *maps = material.maps;
  if (maps[material_map_index(MaterialMap::Albedo)].color.a == 0) {
    maps[material_map_index(MaterialMap::Albedo)].color = gl::kWhite;
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

inline auto apply_pbr_fallback_textures(gl::Material &material) -> void {
  auto &fallbacks = pbr_fallback_textures();

  auto *maps = material.maps;
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

inline auto resolve_shader(NonSendMarker, ShaderCache &cache,
                           const ShaderSpec &spec) -> ShaderProgram & {
  auto [it, inserted] = cache.shaders.try_emplace(spec);
  if (inserted) {
    it->second = ShaderProgram::load(spec);
  }
  return it->second;
}

inline auto initialize_material_map_location(ShaderProgram &shader,
                                             MaterialMap map) -> void {
  auto &raw = shader.raw();
  if (raw.locs == nullptr) {
    return;
  }

  const int location_index = shader_location_index(map);
  if (raw.locs[location_index] < 0) {
    raw.locs[location_index] =
        shader.uniform_location(map_texture_binding(map));
  }
}

inline auto initialize_material_map_locations(ShaderProgram &shader) -> void {
  initialize_material_map_location(shader, MaterialMap::Albedo);
  initialize_material_map_location(shader, MaterialMap::Metalness);
  initialize_material_map_location(shader, MaterialMap::Normal);
  initialize_material_map_location(shader, MaterialMap::Roughness);
  initialize_material_map_location(shader, MaterialMap::Occlusion);
  initialize_material_map_location(shader, MaterialMap::Emission);
}

inline auto apply_material_map_binding(gl::Material &material,
                                       const MaterialMapBinding &binding,
                                       const Assets<Texture> &textures)
    -> void {
  const int map_index = material_map_index(binding.map_type);
  if (map_index < 0 || map_index >= kMaterialMapCount) {
    return;
  }

  auto &map = material.maps[map_index];
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

inline auto lookup_slot_provider(const SlotProviderRegistry &registry,
                                 const MaterialSlotRequirement &slot,
                                 const CompiledMaterial &material)
    -> const SlotProvider & {
  auto iter = registry.providers.find(slot.type);
  if (iter != registry.providers.end()) {
    return iter->second;
  }

  throw std::runtime_error(
      "Missing slot provider for '" + slot.debug_name +
      "' required by material '" + material.material_type_name + "' (" +
      material.shader.vertex_path + ", " + material.shader.fragment_path + ")");
}

inline auto prepare_material(NonSendMarker marker,
                             const CompiledMaterial &material,
                             const Assets<Texture> &texture_assets,
                             ShaderCache &shader_cache,
                             const SlotProviderRegistry &slot_registry,
                             const SlotPrepareContext &prepare_ctx,
                             const ShaderSpec &shader_spec,
                             PreparedMaterial &prepared) -> ShaderProgram & {
  const auto &src_mat = gl::Material::fallback_material(marker);
  prepared.material = src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat.maps,
              sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();

  ShaderProgram &shader = resolve_shader(marker, shader_cache, shader_spec);
  initialize_material_map_locations(shader);
  prepared.material.shader = shader.raw();

  for (const auto &binding : material.maps) {
    apply_material_map_binding(prepared.material, binding, texture_assets);
  }

  for (const auto &slot : material.slots) {
    const auto &provider = lookup_slot_provider(slot_registry, slot, material);
    if (provider.prepare) {
      provider.prepare(prepare_ctx, material, prepared, shader);
    }
  }

  return shader;
}

inline auto apply_slot_uniforms(const SlotProviderRegistry &slot_registry,
                                const RenderSlotContext &render_ctx,
                                const CompiledMaterial &material,
                                const PreparedMaterial &prepared,
                                ShaderProgram &shader) -> void {
  for (const auto &slot : material.slots) {
    const auto &provider = lookup_slot_provider(slot_registry, slot, material);
    if (provider.apply) {
      provider.apply(render_ctx, material, prepared, shader);
    }
  }
}

inline auto make_has_camera_slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.apply = [](const RenderSlotContext &ctx, const CompiledMaterial &,
                      const PreparedMaterial &, ShaderProgram &shader) -> void {
    shader.set(CameraUniform::Position,
               math::to_rl(ctx.view().camera_view.position));
    shader.set(CameraUniform::ViewMatrix, ctx.view().camera_view.view);
  };
  return provider;
}

inline auto make_has_pbr_slot_provider() -> SlotProvider {
  SlotProvider provider;
  provider.prepare = [](const SlotPrepareContext &, const CompiledMaterial &,
                        PreparedMaterial &prepared, ShaderProgram &) -> void {
    apply_pbr_defaults(prepared.material);
    apply_pbr_fallback_textures(prepared.material);
  };
  provider.apply = [](const RenderSlotContext &ctx, const CompiledMaterial &,
                      const PreparedMaterial &, ShaderProgram &shader) -> void {
    const auto &view = ctx.view();
    const auto &lights = ctx.lights();
    const auto &cluster_config = ctx.cluster_config();

    int has_directional = lights.has_directional ? 1 : 0;
    gl::Vec3 dir_color = color_to_vec3(lights.dir_light.color);
    gl::Vec3 dir_dir = math::to_rl(lights.dir_light.direction.normalized());
    float dir_intensity = lights.dir_light.intensity;
    gl::Vec2 cluster_screen_size = {
        static_cast<float>(std::max(gl::screen_width(), 1)),
        static_cast<float>(std::max(gl::screen_height(), 1)),
    };
    gl::Vec2 cluster_near_far = {
        std::max(view.near_plane, 0.001f),
        std::max(view.far_plane, view.near_plane + 0.001f),
    };
    std::array<int, 4> cluster_dimensions = {cluster_config.tile_count_x,
                                             cluster_config.tile_count_y,
                                             cluster_config.slice_count_z, 0};
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
inline auto draw_batch(ShaderProgram &shader, const gl::Mesh &mesh,
                       gl::Material &material, const BatchT &batch) -> void {
  (void)shader;
  mesh.draw_instanced(material, batch.transforms.data(),
                      static_cast<i32>(batch.transforms.size()));
}

} // namespace ecs
