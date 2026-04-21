#pragma once

#include "types.hpp"
#include <concepts>
#include <string_view>
#include <type_traits>

namespace ecs {

enum class MaterialMap : u8 {
  Albedo = 0,
  Metalness = 1,
  Normal = 2,
  Roughness = 3,
  Occlusion = 4,
  Emission = 5,
  Height = 6,
  Cubemap = 7,
  Irradiance = 8,
  Prefilter = 9,
  Brdf = 10,
};

inline constexpr i32 K_MATERIAL_MAP_COUNT = 12;
inline constexpr i32 K_RAYLIB_MAP_LOCATION_BASE = 15;

constexpr auto material_map_index(MaterialMap map) -> i32 {
  return static_cast<int>(map);
}

constexpr auto shader_location_index(MaterialMap map) -> i32 {
  return K_RAYLIB_MAP_LOCATION_BASE + material_map_index(map);
}

inline auto map_texture_binding(MaterialMap map) -> std::string_view {
  switch (map) {
  case MaterialMap::Albedo:
    return "texture0";
  case MaterialMap::Metalness:
    return "texture1";
  case MaterialMap::Normal:
    return "texture2";
  case MaterialMap::Roughness:
    return "u_roughness_texture";
  case MaterialMap::Occlusion:
    return "u_occlusion_texture";
  case MaterialMap::Emission:
    return "u_emissive_texture";
  case MaterialMap::Height:
    return "texture6";
  case MaterialMap::Cubemap:
    return "texture7";
  case MaterialMap::Irradiance:
    return "texture8";
  case MaterialMap::Prefilter:
    return "texture9";
  case MaterialMap::Brdf:
    return "texture10";
  }
  return "";
}

inline auto map_uniform_binding(MaterialMap map) -> std::string_view {
  switch (map) {
  case MaterialMap::Albedo:
    return "u_color";
  case MaterialMap::Metalness:
    return "u_metallic_factor";
  case MaterialMap::Normal:
    return "u_normal_factor";
  case MaterialMap::Roughness:
    return "u_roughness_factor";
  case MaterialMap::Occlusion:
    return "u_occlusion_factor";
  case MaterialMap::Emission:
    return "u_emissive_factor";
  case MaterialMap::Height:
    return "u_height_factor";
  case MaterialMap::Cubemap:
    return "u_cubemap_factor";
  case MaterialMap::Irradiance:
    return "u_irradiance_factor";
  case MaterialMap::Prefilter:
    return "u_prefilter_factor";
  case MaterialMap::Brdf:
    return "u_brdf_factor";
  }
  return "";
}

enum class StandardMaterialUniform {
  AlphaCutoff,
  RenderMethod,
};

inline auto map_uniform_binding(StandardMaterialUniform uniform)
  -> std::string_view {
  switch (uniform) {
  case StandardMaterialUniform::AlphaCutoff:
    return "alphaCutoff";
  case StandardMaterialUniform::RenderMethod:
    return "u_render_method";
  }
  return "";
}

enum class CameraUniform {
  Position,
  ViewMatrix,
  ProjectionMatrix,
};

inline auto map_uniform_binding(CameraUniform uniform) -> std::string_view {
  switch (uniform) {
  case CameraUniform::Position:
    return "cameraPos";
  case CameraUniform::ViewMatrix:
    return "matView";
  case CameraUniform::ProjectionMatrix:
    return "matProjection";
  }
  return "";
}

enum class PbrLightingUniform {
  HasDirectional,
  DirectionalDirection,
  DirectionalIntensity,
  DirectionalColor,
  ClusterDimensions,
  ClusterScreenSize,
  ClusterNearFar,
  ClusterMaxLights,
  IsOrthographic,
};

inline auto map_uniform_binding(PbrLightingUniform uniform)
  -> std::string_view {
  switch (uniform) {
  case PbrLightingUniform::HasDirectional:
    return "u_has_directional";
  case PbrLightingUniform::DirectionalDirection:
    return "u_dir_light_direction";
  case PbrLightingUniform::DirectionalIntensity:
    return "u_dir_light_intensity";
  case PbrLightingUniform::DirectionalColor:
    return "u_dir_light_color";
  case PbrLightingUniform::ClusterDimensions:
    return "u_cluster_dimensions";
  case PbrLightingUniform::ClusterScreenSize:
    return "u_cluster_screen_size";
  case PbrLightingUniform::ClusterNearFar:
    return "u_cluster_near_far";
  case PbrLightingUniform::ClusterMaxLights:
    return "u_cluster_max_lights";
  case PbrLightingUniform::IsOrthographic:
    return "u_is_orthographic";
  }
  return "";
}

template <typename T>
concept ShaderTextureBinding = std::is_enum_v<T> && requires(T value) {
  { map_texture_binding(value) } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept ShaderUniformBinding = std::is_enum_v<T> && requires(T value) {
  { map_uniform_binding(value) } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept ShaderBindings = ShaderTextureBinding<T> || ShaderUniformBinding<T>;

} // namespace ecs
