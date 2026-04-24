#pragma once

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/components/environment.hpp"
#include "rydz_graphics/components/light.hpp"
#include "rydz_graphics/components/mesh3d.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/material/postprocess_material.hpp"
#include "rydz_graphics/pipeline/batches.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include <vector>

namespace ecs {

struct ExtractedView {
  using T = Resource;
  gl::Rectangle viewport{0.0F, 0.0F, 1.0F, 1.0F};
  CameraView camera_view{
    .view = Mat4::IDENTITY,
    .proj = Mat4::IDENTITY,
    .position = Vec3{},
  };
  Color clear_color = ClearColor{}.color;
  Environment const* active_environment{};
  PostProcessDescriptor postprocess{};
  bool active{};
  bool has_postprocess{};
  bool orthographic{};
  float near_plane{0.1F};
  float far_plane{1000.0F};

  auto clear() -> void {
    active_environment = nullptr;
    postprocess = {};
    active = false;
    has_postprocess = false;
  }
};

struct ExtractedLights {
  using T = Resource;

  struct PointLight {
    Vec3 position;
    Color color{Color::WHITE};
    float intensity{0.0F};
    float range{0.0F};
  };

  std::vector<PointLight> point_lights{};
  DirectionalLight dir_light{};
  bool has_directional{};

  auto clear() -> void {
    point_lights.clear();
    dir_light = {};
    has_directional = false;
  }
};

struct ExtractedMeshes {
  using T = Resource;

  struct MaterialItem {
    RenderMaterialKey key{};
    CompiledMaterial material{};
    bool transparent{};
    bool casts_shadows{true};
  };

  struct Item {
    Handle<Mesh> mesh{};
    RenderMaterialKey material{};
    usize material_index{};
    Mat4 world_transform{Mat4::IDENTITY};
    float distance_sq_to_camera{0.0F};
  };

  std::vector<MaterialItem> materials;
  std::vector<Item> items;
  std::unordered_map<u32, usize> material_lookup;

  auto clear() -> void {
    materials.clear();
    items.clear();
    material_lookup.clear();
  }
};

struct ExtractedUi {
  using T = Resource;

  struct Item {
    Handle<Texture> texture{};
    Transform transform{};
    Color tint = Color::WHITE;
    i32 layer = 0;
  };

  std::vector<Item> items{};
  auto clear() -> void { items.clear(); }
};

} // namespace ecs
