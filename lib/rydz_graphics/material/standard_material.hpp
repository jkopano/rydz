#pragma once

#include "material3d.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/components/color.hpp"

namespace ecs {

struct StandardMaterial : MaterialTrait<HasPBR> {
  Color base_color = Color::WHITE;
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

  static auto from_color(Color c) -> StandardMaterial { return {.base_color = c}; }
  static auto from_texture(Handle<Texture> tex, Color tint = Color::WHITE)
    -> StandardMaterial {
    return {.base_color = tint, .texture = tex};
  }

  static auto vertex_shader() -> ShaderRef { return "res/shaders/pbr.vert"; }
  static auto fragment_shader() -> ShaderRef { return "res/shaders/pbr.frag"; }

  [[nodiscard]] auto render_method() const -> RenderMethod {
    return base_color.a < 255 ? RenderMethod::Transparent : RenderMethod::Opaque;
  }

  [[nodiscard]] auto enable_shadows() const -> bool { return base_color.a == 255; }
  [[nodiscard]] auto double_sided() const -> bool { return false; }
  [[nodiscard]] auto alpha_cutoff() const -> float { return 0.1F; }

  auto bind(MaterialBuilder& builder) const -> void {
    builder.uniform(MaterialMap::Albedo, base_color);
    if (texture.is_valid()) {
      builder.texture(MaterialMap::Albedo, texture);
    }
    if (normal_map.is_valid()) {
      builder.texture(MaterialMap::Normal, normal_map);
    }
    if (metallic_map.is_valid()) {
      builder.texture(MaterialMap::Metalness, metallic_map);
    }
    if (roughness_map.is_valid()) {
      builder.texture(MaterialMap::Roughness, roughness_map);
    }
    if (occlusion_map.is_valid()) {
      builder.texture(MaterialMap::Occlusion, occlusion_map);
    }
    if (emissive_map.is_valid()) {
      builder.texture(MaterialMap::Emission, emissive_map);
    }

    if (metallic >= 0.0F) {
      builder.uniform(MaterialMap::Metalness, gl::Uniform(metallic));
    }
    if (roughness >= 0.0F) {
      builder.uniform(MaterialMap::Roughness, gl::Uniform{roughness});
    }
    if (normal_scale >= 0.0F) {
      builder.uniform(MaterialMap::Normal, gl::Uniform{normal_scale});
    }
    if (occlusion_strength >= 0.0F) {
      builder.uniform(MaterialMap::Occlusion, gl::Uniform{occlusion_strength});
    }
    if (emissive_color.a > 0) {
      math::Vec3 emissive = emissive_color;
      builder.uniform(
        MaterialMap::Emission, math::Vec3(emissive.x, emissive.y, emissive.z)
      );
    }
    builder.uniform(MaterialMap::Albedo, gl::Uniform{1.0F, 1.0F, 1.0F, 0.0F});
  }
};

static_assert(MaterialValue<StandardMaterial>);

} // namespace ecs
