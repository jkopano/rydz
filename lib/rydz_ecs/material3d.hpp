#pragma once
#include "asset.hpp"
#include <concepts>
#include <raylib-cpp/raylib-cpp.hpp>

namespace ecs {

template <typename M>
concept Material =
    requires(const M m, Model &model, Assets<Texture2D> *textures) {
      { m.color() } -> std::convertible_to<Color>;
      { m.apply(model, textures) } -> std::same_as<void>;
    };

struct StandardMaterial {
  Color base_color = WHITE;
  Handle<Texture2D> texture{};
  Handle<Texture2D> normal_map{};
  float metallic = 0.0f;
  float roughness = 0.5f;

  static StandardMaterial from_color(Color c) { return {.base_color = c}; }

  Color color() const { return base_color; }

  void apply(Model &model, Assets<Texture2D> *tex_assets) const {
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = base_color;

    if (tex_assets && texture.is_valid()) {
      Texture2D *tex = tex_assets->get(texture);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
    }

    if (tex_assets && normal_map.is_valid()) {
      Texture2D *tex = tex_assets->get(normal_map);
      if (tex)
        model.materials[0].maps[MATERIAL_MAP_NORMAL].texture = *tex;
    }
  }
};

static_assert(Material<StandardMaterial>);

template <Material M> struct MeshMaterial3d {
  M material;

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(M m) : material(std::move(m)) {}
  explicit MeshMaterial3d(Color c)
    requires std::constructible_from<M, Color>
      : material{c} {}
};

using Material3d = MeshMaterial3d<StandardMaterial>;

} // namespace ecs
