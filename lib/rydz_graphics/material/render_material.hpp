#pragma once

#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/buffers.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/material/slot_provider.hpp"
#include "rydz_graphics/pipeline/batches.hpp"
#include "rydz_graphics/pipeline/graph.hpp"
#include "rydz_graphics/pipeline/pass_context.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ecs {

inline constexpr unsigned int VIEW_UNIFORM_BINDING = 0;
inline constexpr std::string_view VIEW_UNIFORM_BLOCK_NAME = "ViewUniforms";

struct alignas(16) ViewUniformData {
  Vec4 camera_position = Vec4::ZERO;
  Mat4 view = Mat4::IDENTITY;
  Mat4 projection = Mat4::IDENTITY;
  Vec4 directional_direction = Vec4::ZERO;
  Vec4 directional_color_intensity = Vec4::ZERO;
  Vec4 ambient_color_intensity = Vec4::ZERO;
  std::array<int, 4> cluster_dimensions = {0, 0, 0, 0};
  Vec4 cluster_screen_size_near_far = Vec4::ZERO;
  std::array<int, 4> view_flags = {0, 0, 0, 0};
};

static_assert(sizeof(ViewUniformData) % 16 == 0);

struct ViewUniformState {
  using T = Resource;

  gl::UBO buffer{};
  ViewUniformData data{};

  auto update(
    ExtractedView const& view,
    ExtractedLights const& lights,
    ClusterConfig const& cluster_config
  ) -> void {
    data.camera_position = Vec4{
      view.camera_view.position.x,
      view.camera_view.position.y,
      view.camera_view.position.z,
      0.0F,
    };
    data.view = view.camera_view.view;
    data.projection = view.camera_view.proj;

    Vec3 const light_direction = lights.dir_light.direction.normalized();
    data.directional_direction = Vec4{
      light_direction.x,
      light_direction.y,
      light_direction.z,
      0.0F,
    };
    data.directional_color_intensity = Vec4{
      lights.dir_light.color.r / 255.0F,
      lights.dir_light.color.g / 255.0F,
      lights.dir_light.color.b / 255.0F,
      lights.dir_light.intensity,
    };
    data.ambient_color_intensity = Vec4{
      lights.ambient_light.color.r / 255.0F,
      lights.ambient_light.color.g / 255.0F,
      lights.ambient_light.color.b / 255.0F,
      lights.ambient_light.intensity,
    };
    data.cluster_dimensions = {
      cluster_config.tile_count_x_clamped(),
      cluster_config.tile_count_y_clamped(),
      cluster_config.slice_count_z_clamped(),
      0,
    };
    data.cluster_screen_size_near_far = Vec4{
      std::max(view.viewport.width, 1.0F),
      std::max(view.viewport.height, 1.0F),
      std::max(view.near_plane, 0.001F),
      std::max(view.far_plane, view.near_plane + 0.001F),
    };
    data.view_flags = {
      lights.has_directional ? 1 : 0,
      static_cast<int>(cluster_config.max_lights_per_cluster_clamped()),
      view.orthographic ? 1 : 0,
      0,
    };

    if (!buffer.ready()) {
      buffer = gl::UBO(
        static_cast<unsigned int>(sizeof(ViewUniformData)), &data, RL_DYNAMIC_DRAW
      );
    } else {
      buffer.update(&data, static_cast<unsigned int>(sizeof(ViewUniformData)), 0);
    }
  }

  auto bind() const -> void {
    if (buffer.ready()) {
      buffer.bind(VIEW_UNIFORM_BINDING);
    }
  }

  auto bind_shader(ShaderProgram& shader) const -> bool {
    if (!buffer.ready()) {
      return false;
    }
    bind();
    return shader.bind_uniform_block(VIEW_UNIFORM_BLOCK_NAME, VIEW_UNIFORM_BINDING);
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

inline auto MaterialContext::frame() const -> PassContext const& { return *frame_data; }

inline auto MaterialContext::textures() const -> Assets<Texture> const& {
  return frame().texture_assets;
}

inline auto MaterialContext::view() const -> ExtractedView const& { return frame().view; }

inline auto MaterialContext::lights() const -> ExtractedLights const& {
  return frame().lights;
}

inline auto MaterialContext::cluster_config() const -> ClusterConfig const& {
  return frame().cluster_config;
}

inline auto MaterialContext::clustered_lighting() const -> ClusteredLightingState const& {
  return frame().cluster_state;
}

inline auto MaterialContext::time() const -> Time const& { return frame().time; }

inline auto MaterialContext::view_uniforms() const -> ViewUniformState const& {
  return frame().view_uniforms;
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
    if (material_map.texture.id > 0) {
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
  provider.apply_per_view = [](
                              MaterialContext const& ctx, ShaderProgram& shader
                            ) -> void { ctx.view_uniforms().bind_shader(shader); };
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
    (void) shader;
    ctx.clustered_lighting().bind();
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
