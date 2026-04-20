#pragma once

#include "material3d.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/color.hpp"
#include "rydz_graphics/gl/core.hpp"

namespace ecs {

struct StandardMaterial : MaterialTrait<HasPBR> {
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

  static StandardMaterial from_color(Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<Texture> tex,
                                       Color tint = kWhite) {
    return {.base_color = tint, .texture = tex};
  }

  static ShaderRef vertex_shader() { return "res/shaders/pbr.vert"; }
  static ShaderRef fragment_shader() { return "res/shaders/pbr.frag"; }

  RenderMethod render_method() const {
    return base_color.a < 255 ? RenderMethod::Transparent
                              : RenderMethod::Opaque;
  }

  bool enable_shadows() const { return base_color.a == 255; }
  bool double_sided() const { return false; }
  float alpha_cutoff() const { return 0.1f; }

  void bind(MaterialBuilder &builder) const {
    builder.color(gl::MATERIAL_MAP_DIFFUSE, base_color);
    if (texture.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_DIFFUSE, texture);
    }
    if (normal_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_NORMAL, normal_map);
    }
    if (metallic_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_METALNESS, metallic_map);
    }
    if (roughness_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_ROUGHNESS, roughness_map);
    }
    if (occlusion_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_OCCLUSION, occlusion_map);
    }
    if (emissive_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_EMISSION, emissive_map);
    }

    if (metallic >= 0.0f) {
      builder.uniform("u_metallic_factor", gl::Uniform(metallic));
    }
    if (roughness >= 0.0f) {
      builder.uniform("u_roughness_factor", gl::Uniform{roughness});
    }
    if (normal_scale >= 0.0f) {
      builder.uniform("u_normal_factor", gl::Uniform{normal_scale});
    }
    if (occlusion_strength >= 0.0f) {
      builder.uniform("u_occlusion_factor", gl::Uniform{occlusion_strength});
    }
    if (emissive_color.a > 0) {
      auto emissive = color_to_vec3(emissive_color);
      builder.uniform("u_emissive_factor",
                      math::Vec3(emissive.x, emissive.y, emissive.z));
    }
    builder.uniform("u_color", gl::Uniform{1.0f, 1.0f, 1.0f, 0.0f});
  }
};

static_assert(MaterialValue<StandardMaterial>);

struct MeshMaterial3d {
  Handle<Material> material{};

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(Handle<Material> material) : material(material) {}
};

} // namespace ecs
