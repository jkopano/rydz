#pragma once

#include "rydz_graphics/clustered_lighting.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/render_batches.hpp"
#include "rydz_graphics/render_extract.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace ecs {

struct PreparedMaterial {
  std::array<gl::MaterialMap, 12> local_maps{};
  gl::Material material{};
};

struct SlotPrepareContext {
  const Assets<Texture> *texture_assets = nullptr;
  bool instanced = false;

  const Assets<Texture> &textures() const { return *texture_assets; }
};

struct RenderSlotContext {
  const ExtractedView *view_data = nullptr;
  const ExtractedLights *lights_data = nullptr;
  const ClusterConfig *cluster_config_data = nullptr;
  const ClusteredLightingState *cluster_state_data = nullptr;
  bool instanced = false;

  const ExtractedView &view() const { return *view_data; }
  const ExtractedLights &lights() const { return *lights_data; }
  const ClusterConfig &cluster_config() const { return *cluster_config_data; }
  const ClusteredLightingState &clustered_lighting() const {
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

inline gl::Material &fallback_material(NonSendMarker) {
  static gl::Material fallback = {};
  static bool init = false;
  if (!init) {
    fallback.shader.id = gl::default_shader_id();
    fallback.shader.locs = gl::default_shader_locs();
    fallback.maps = static_cast<gl::MaterialMap *>(
        std::calloc(12, sizeof(gl::MaterialMap)));
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.id =
        gl::default_texture_id();
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.width = 1;
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.height = 1;
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.mipmaps = 1;
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.format =
        gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    fallback.maps[gl::MATERIAL_MAP_DIFFUSE].color = gl::kWhite;
    init = true;
  }
  return fallback;
}

inline PbrFallbackTextures &pbr_fallback_textures() {
  static PbrFallbackTextures textures = [] {
    auto make_texture = [](gl::Color color) {
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

inline void apply_pbr_defaults(gl::Material &material) {
  if (material.maps[gl::MATERIAL_MAP_DIFFUSE].color.a == 0) {
    material.maps[gl::MATERIAL_MAP_DIFFUSE].color = gl::kWhite;
  }
  if (material.maps[gl::MATERIAL_MAP_ROUGHNESS].value <= 0.0F &&
      !material.maps[gl::MATERIAL_MAP_ROUGHNESS].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_ROUGHNESS].value = 1.0F;
  }
  if (material.maps[gl::MATERIAL_MAP_NORMAL].value <= 0.0f) {
    material.maps[gl::MATERIAL_MAP_NORMAL].value = 1.0f;
  }
  if (material.maps[gl::MATERIAL_MAP_OCCLUSION].value <= 0.0f) {
    material.maps[gl::MATERIAL_MAP_OCCLUSION].value = 1.0f;
  }
}

inline void apply_pbr_fallback_textures(gl::Material &material) {
  auto &fallbacks = pbr_fallback_textures();

  if (!material.maps[gl::MATERIAL_MAP_METALNESS].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_METALNESS].texture =
        fallbacks.metallic_black;
  }
  if (!material.maps[gl::MATERIAL_MAP_ROUGHNESS].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_ROUGHNESS].texture =
        fallbacks.roughness_white;
  }
  if (!material.maps[gl::MATERIAL_MAP_NORMAL].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_NORMAL].texture = fallbacks.normal_flat;
  }
  if (!material.maps[gl::MATERIAL_MAP_OCCLUSION].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_OCCLUSION].texture =
        fallbacks.occlusion_white;
  }
  if (!material.maps[gl::MATERIAL_MAP_EMISSION].texture.ready()) {
    material.maps[gl::MATERIAL_MAP_EMISSION].texture = fallbacks.emission_black;
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

  if (raw.locs[gl::SHADER_LOC_MAP_ROUGHNESS] < 0) {
    raw.locs[gl::SHADER_LOC_MAP_ROUGHNESS] =
        gl::shader_location(raw, "u_roughness_texture");
  }
  if (raw.locs[gl::SHADER_LOC_MAP_OCCLUSION] < 0) {
    raw.locs[gl::SHADER_LOC_MAP_OCCLUSION] =
        gl::shader_location(raw, "u_occlusion_texture");
  }
  if (raw.locs[gl::SHADER_LOC_MAP_EMISSION] < 0) {
    raw.locs[gl::SHADER_LOC_MAP_EMISSION] =
        gl::shader_location(raw, "u_emissive_texture");
  }
}

inline void apply_material_map_binding(gl::Material &material,
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

inline const SlotProvider &
lookup_slot_provider(const SlotProviderRegistry &registry,
                     const MaterialSlotRequirement &slot,
                     const CompiledMaterial &material) {
  auto it = registry.providers.find(slot.type);
  if (it != registry.providers.end()) {
    return it->second;
  }

  throw std::runtime_error(
      "Missing slot provider for '" + slot.debug_name +
      "' required by material '" + material.material_type_name + "' (" +
      material.shader.vertex_path + ", " + material.shader.fragment_path + ")");
}

inline ShaderProgram &
prepare_material(NonSendMarker marker, const CompiledMaterial &material,
                 const Assets<Texture> &texture_assets,
                 ShaderCache &shader_cache,
                 const SlotProviderRegistry &slot_registry,
                 const SlotPrepareContext &prepare_ctx,
                 const ShaderSpec &shader_spec, PreparedMaterial &prepared) {
  const auto &src_mat = fallback_material(marker);
  prepared.material = src_mat;
  std::memcpy(prepared.local_maps.data(), src_mat.maps,
              sizeof(prepared.local_maps));
  prepared.material.maps = prepared.local_maps.data();

  ShaderProgram &shader = resolve_shader(marker, shader_cache, shader_spec);
  initialize_custom_map_locations(shader);
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

inline void apply_compiled_uniforms(ShaderProgram &shader,
                                    const CompiledMaterial &material) {
  shader.set("u_alpha_cutoff", material.alpha_cutoff);
  shader.set("u_render_method", static_cast<int>(material.render_method));
  shader.set("u_use_instancing", 0);
  for (const auto &[name, uniform] : material.uniforms) {
    shader.apply(std::string(name), uniform);
  }
}

inline void apply_slot_uniforms(const SlotProviderRegistry &slot_registry,
                                const RenderSlotContext &render_ctx,
                                const CompiledMaterial &material,
                                const PreparedMaterial &prepared,
                                ShaderProgram &shader) {
  for (const auto &slot : material.slots) {
    const auto &provider = lookup_slot_provider(slot_registry, slot, material);
    if (provider.apply) {
      provider.apply(render_ctx, material, prepared, shader);
    }
  }
}

inline SlotProvider make_has_camera_slot_provider() {
  SlotProvider provider;
  provider.apply = [](const RenderSlotContext &ctx, const CompiledMaterial &,
                      const PreparedMaterial &, ShaderProgram &shader) {
    shader.set("u_camera_pos", math::to_rl(ctx.view().camera_view.position));
    shader.set("matView", ctx.view().camera_view.view);
  };
  return provider;
}

inline SlotProvider make_has_pbr_slot_provider() {
  SlotProvider provider;
  provider.prepare = [](const SlotPrepareContext &, const CompiledMaterial &,
                        PreparedMaterial &prepared, ShaderProgram &) {
    apply_pbr_defaults(prepared.material);
    apply_pbr_fallback_textures(prepared.material);
  };
  provider.apply = [](const RenderSlotContext &ctx, const CompiledMaterial &,
                      const PreparedMaterial &, ShaderProgram &shader) {
    const auto &view = ctx.view();
    const auto &lights = ctx.lights();
    const auto &cluster_config = ctx.cluster_config();

    int has_directional = lights.has_directional ? 1 : 0;
    gl::Vec3 dir_color = color_to_vec3(lights.dir_light.color);
    gl::Vec3 dir_dir = math::to_rl(lights.dir_light.direction.Normalized());
    float dir_intensity = lights.dir_light.intensity;
    gl::Vec2 cluster_screen_size = {
        static_cast<float>(std::max(gl::screen_width(), 1)),
        static_cast<float>(std::max(gl::screen_height(), 1)),
    };
    gl::Vec2 cluster_near_far = {
        std::max(view.near_plane, 0.001f),
        std::max(view.far_plane, view.near_plane + 0.001f),
    };
    int cluster_dimensions[4] = {cluster_config.tile_count_x,
                                 cluster_config.tile_count_y,
                                 cluster_config.slice_count_z, 0};
    int cluster_max_lights =
        static_cast<int>(cluster_config.max_lights_per_cluster);
    int is_orthographic = view.orthographic ? 1 : 0;

    gl::bind_clustered_lighting(ctx.clustered_lighting());
    shader.set("u_has_directional", has_directional);
    shader.set("u_dir_light_direction", dir_dir);
    shader.set("u_dir_light_intensity", dir_intensity);
    shader.set("u_dir_light_color", dir_color);
    shader.set_ints("u_cluster_dimensions", cluster_dimensions,
                    gl::SHADER_UNIFORM_IVEC4);
    shader.set("u_cluster_screen_size", cluster_screen_size);
    shader.set("u_cluster_near_far", cluster_near_far);
    shader.set("u_cluster_max_lights", cluster_max_lights);
    shader.set("u_is_orthographic", is_orthographic);
  };
  return provider;
}

inline bool can_draw_instanced(gl::Material &material, const gl::Mesh &mesh) {
  if (!material.shader.locs || mesh.vaoId == 0) {
    return false;
  }

  if (material.shader.locs[gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] < 0) {
    material.shader.locs[gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] =
        gl::shader_location_attrib(material.shader, "instanceTransform");
  }
  if (material.shader.locs[gl::SHADER_LOC_MATRIX_MODEL] < 0) {
    material.shader.locs[gl::SHADER_LOC_MATRIX_MODEL] =
        gl::shader_location(material.shader, "matModel");
  }

  return material.shader.locs[gl::SHADER_LOC_VERTEX_INSTANCETRANSFORM] >= 0;
}

template <typename BatchT>
inline void draw_batch(ShaderProgram &shader, const gl::Mesh &mesh,
                       gl::Material &material, const BatchT &batch) {
  shader.set("u_use_instancing", 1);
  gl::draw_mesh_instanced(mesh, material, batch.transforms.data(),
                          static_cast<i32>(batch.transforms.size()));
}

} // namespace ecs
