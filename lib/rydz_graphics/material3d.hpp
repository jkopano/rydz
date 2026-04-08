#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "shader.hpp"
#include <concepts>
#include <utility>
#include <vector>

namespace ecs {

struct MaterialMapBinding {
  MaterialMapIndex map_type = MATERIAL_MAP_DIFFUSE;
  Handle<rl::Texture2D> texture{};
  rl::Color color = WHITE;
  f32 value = 0.0f;
  bool has_texture = false;
  bool has_color = false;
  bool has_value = false;

  bool operator==(const MaterialMapBinding &o) const {
    return map_type == o.map_type && texture == o.texture &&
           color.r == o.color.r && color.g == o.color.g &&
           color.b == o.color.b && color.a == o.color.a && value == o.value &&
           has_texture == o.has_texture && has_color == o.has_color &&
           has_value == o.has_value;
  }

  static MaterialMapBinding texture_binding(MaterialMapIndex map_type,
                                            Handle<rl::Texture2D> texture) {
    return MaterialMapBinding{
        .map_type = map_type,
        .texture = texture,
        .has_texture = texture.is_valid(),
    };
  }

  static MaterialMapBinding color_binding(MaterialMapIndex map_type,
                                          rl::Color color) {
    return MaterialMapBinding{
        .map_type = map_type,
        .color = color,
        .has_color = true,
    };
  }

  static MaterialMapBinding value_binding(MaterialMapIndex map_type,
                                          f32 value) {
    return MaterialMapBinding{
        .map_type = map_type,
        .value = value,
        .has_value = true,
    };
  }
};

struct MaterialFlags {
  bool transparent = false;
  bool casts_shadows = true;
  bool double_sided = false;

  bool operator==(const MaterialFlags &o) const = default;
};

enum class MaterialShadingModel {
  Unlit,
  ClusteredPbr,
};

struct MaterialDescriptor {
  ShaderSpec shader;
  std::vector<MaterialMapBinding> maps;
  std::vector<Uniform> uniforms;
  MaterialFlags flags{};
  MaterialShadingModel shading_model = MaterialShadingModel::Unlit;

  bool operator==(const MaterialDescriptor &o) const = default;
};

template <typename M>
concept IsMaterial = requires(const M &m) {
  { m.describe() } -> std::same_as<MaterialDescriptor>;
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
  f32 metallic = -1.0f;
  f32 roughness = -1.0f;
  f32 normal_scale = -1.0f;
  f32 occlusion_strength = -1.0f;

  static StandardMaterial from_color(rl::Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<rl::Texture2D> tex,
                                       rl::Color tint = WHITE) {
    return {.base_color = tint, .texture = tex};
  }

  MaterialDescriptor describe() const {
    MaterialDescriptor descriptor;
    descriptor.shader =
        ShaderSpec::from("res/shaders/pbr.vert", "res/shaders/pbr.frag");
    descriptor.flags.transparent = base_color.a < 255;
    descriptor.flags.casts_shadows = base_color.a == 255;
    descriptor.shading_model = MaterialShadingModel::ClusteredPbr;

    descriptor.maps.push_back(
        MaterialMapBinding::color_binding(MATERIAL_MAP_DIFFUSE, base_color));

    if (texture.is_valid()) {
      descriptor.maps.push_back(
          MaterialMapBinding::texture_binding(MATERIAL_MAP_DIFFUSE, texture));
    }
    if (normal_map.is_valid()) {
      descriptor.maps.push_back(
          MaterialMapBinding::texture_binding(MATERIAL_MAP_NORMAL, normal_map));
    }
    if (metallic_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          MATERIAL_MAP_METALNESS, metallic_map));
    }
    if (roughness_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          MATERIAL_MAP_ROUGHNESS, roughness_map));
    }
    if (occlusion_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          MATERIAL_MAP_OCCLUSION, occlusion_map));
    }
    if (emissive_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          MATERIAL_MAP_EMISSION, emissive_map));
    }

    if (metallic >= 0.0f) {
      descriptor.maps.push_back(
          MaterialMapBinding::value_binding(MATERIAL_MAP_METALNESS, metallic));
    }
    if (roughness >= 0.0f) {
      descriptor.maps.push_back(
          MaterialMapBinding::value_binding(MATERIAL_MAP_ROUGHNESS, roughness));
    }
    if (normal_scale >= 0.0f) {
      descriptor.maps.push_back(
          MaterialMapBinding::value_binding(MATERIAL_MAP_NORMAL, normal_scale));
    }
    if (occlusion_strength >= 0.0f) {
      descriptor.maps.push_back(MaterialMapBinding::value_binding(
          MATERIAL_MAP_OCCLUSION, occlusion_strength));
    }
    if (emissive_color.a > 0) {
      descriptor.maps.push_back(MaterialMapBinding::color_binding(
          MATERIAL_MAP_EMISSION, emissive_color));
    }

    return descriptor;
  }
};

static_assert(IsMaterial<StandardMaterial>);

template <IsMaterial M = StandardMaterial> struct MeshMaterial3d {
  M material;

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(M m) : material(std::move(m)) {}
  explicit MeshMaterial3d(rl::Color c)
    requires std::constructible_from<M, rl::Color>
      : material{c} {}
};

} // namespace ecs
