#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <array>
#include <concepts>
#include <string>
#include <utility>
#include <vector>

namespace ecs {

struct ShaderProgramSpec {
  std::string vertex_path;
  std::string fragment_path;

  bool operator==(const ShaderProgramSpec &o) const = default;
};

enum class ShaderUniformType {
  Float,
  Vec2,
  Vec3,
  Vec4,
  Int,
  IVec2,
  IVec3,
  IVec4,
  Mat4,
};

struct ShaderUniformValue {
  std::string name;
  ShaderUniformType type = ShaderUniformType::Float;
  std::array<float, 16> float_data{};
  std::array<int, 4> int_data{};
  int count = 1;

  bool operator==(const ShaderUniformValue &o) const = default;

  static ShaderUniformValue float1(std::string name, f32 value) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Float;
    uniform.float_data[0] = value;
    return uniform;
  }

  static ShaderUniformValue vec2(std::string name, f32 x, f32 y) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Vec2;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    return uniform;
  }

  static ShaderUniformValue vec3(std::string name, f32 x, f32 y, f32 z) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Vec3;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    uniform.float_data[2] = z;
    return uniform;
  }

  static ShaderUniformValue vec4(std::string name, f32 x, f32 y, f32 z, f32 w) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Vec4;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    uniform.float_data[2] = z;
    uniform.float_data[3] = w;
    return uniform;
  }

  static ShaderUniformValue int1(std::string name, int value) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Int;
    uniform.int_data[0] = value;
    return uniform;
  }

  static ShaderUniformValue mat4(std::string name,
                                 const std::array<float, 16> &value) {
    ShaderUniformValue uniform;
    uniform.name = std::move(name);
    uniform.type = ShaderUniformType::Mat4;
    uniform.float_data = value;
    return uniform;
  }
};

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
  ShaderProgramSpec shader;
  std::vector<MaterialMapBinding> maps;
  std::vector<ShaderUniformValue> uniforms;
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
    descriptor.shader = {
        .vertex_path = "res/shaders/pbr.vert",
        .fragment_path = "res/shaders/pbr.frag",
    };
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
