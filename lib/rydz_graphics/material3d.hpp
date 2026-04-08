#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets.hpp"
#include "rydz_graphics/shader.hpp"
#include "rydz_graphics/types.hpp"
#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace ecs {

struct MaterialMapBinding {
  rydz_gl::MaterialMapIndex map_type = rydz_gl::MATERIAL_MAP_DIFFUSE;
  Handle<Texture> texture{};
  Color color = kWhite;
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

  static MaterialMapBinding
  texture_binding(rydz_gl::MaterialMapIndex map_type,
                  Handle<Texture> texture) {
    return MaterialMapBinding{
        .map_type = map_type,
        .texture = texture,
        .has_texture = texture.is_valid(),
    };
  }

  static MaterialMapBinding color_binding(rydz_gl::MaterialMapIndex map_type,
                                          Color color) {
    return MaterialMapBinding{
        .map_type = map_type,
        .color = color,
        .has_color = true,
    };
  }

  static MaterialMapBinding value_binding(rydz_gl::MaterialMapIndex map_type,
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
concept MaterialValue = requires(const M &m) {
  { m.describe() } -> std::same_as<MaterialDescriptor>;
};

struct Material {
  MaterialDescriptor descriptor{};

  Material() = default;
  explicit Material(MaterialDescriptor descriptor)
      : descriptor(std::move(descriptor)) {}
  template <MaterialValue M>
    requires(!std::same_as<std::remove_cvref_t<M>, Material>)
  Material(const M &material) : descriptor(material.describe()) {}
};

struct StandardMaterial {
  Color base_color = kWhite;
  Handle<Texture> texture{};
  Handle<Texture> normal_map{};
  Handle<Texture> metallic_map{};
  Handle<Texture> roughness_map{};
  Handle<Texture> occlusion_map{};
  Handle<Texture> emissive_map{};
  Color emissive_color = {0, 0, 0, 0};
  f32 metallic = -1.0f;
  f32 roughness = -1.0f;
  f32 normal_scale = -1.0f;
  f32 occlusion_strength = -1.0f;

  static StandardMaterial from_color(Color c) {
    return {.base_color = c};
  }
  static StandardMaterial from_texture(Handle<Texture> tex,
                                       Color tint = kWhite) {
    return {.base_color = tint, .texture = tex};
  }

  MaterialDescriptor describe() const {
    MaterialDescriptor descriptor;
    descriptor.shader =
        ShaderSpec::from("res/shaders/pbr.vert", "res/shaders/pbr.frag");
    descriptor.flags.transparent = base_color.a < 255;
    descriptor.flags.casts_shadows = base_color.a == 255;
    descriptor.shading_model = MaterialShadingModel::ClusteredPbr;

    descriptor.maps.push_back(MaterialMapBinding::color_binding(
        rydz_gl::MATERIAL_MAP_DIFFUSE, base_color));

    if (texture.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_DIFFUSE, texture));
    }
    if (normal_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_NORMAL, normal_map));
    }
    if (metallic_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_METALNESS, metallic_map));
    }
    if (roughness_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_ROUGHNESS, roughness_map));
    }
    if (occlusion_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_OCCLUSION, occlusion_map));
    }
    if (emissive_map.is_valid()) {
      descriptor.maps.push_back(MaterialMapBinding::texture_binding(
          rydz_gl::MATERIAL_MAP_EMISSION, emissive_map));
    }

    if (metallic >= 0.0f) {
      descriptor.maps.push_back(MaterialMapBinding::value_binding(
          rydz_gl::MATERIAL_MAP_METALNESS, metallic));
    }
    if (roughness >= 0.0f) {
      descriptor.maps.push_back(MaterialMapBinding::value_binding(
          rydz_gl::MATERIAL_MAP_ROUGHNESS, roughness));
    }
    if (normal_scale >= 0.0f) {
      descriptor.maps.push_back(MaterialMapBinding::value_binding(
          rydz_gl::MATERIAL_MAP_NORMAL, normal_scale));
    }
    if (occlusion_strength >= 0.0f) {
      descriptor.maps.push_back(MaterialMapBinding::value_binding(
          rydz_gl::MATERIAL_MAP_OCCLUSION, occlusion_strength));
    }
    if (emissive_color.a > 0) {
      descriptor.maps.push_back(MaterialMapBinding::color_binding(
          rydz_gl::MATERIAL_MAP_EMISSION, emissive_color));
    }

    return descriptor;
  }
};

static_assert(MaterialValue<StandardMaterial>);

struct MeshMaterial3d {
  Handle<Material> material{};

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(Handle<Material> material) : material(material) {}
};

} // namespace ecs
