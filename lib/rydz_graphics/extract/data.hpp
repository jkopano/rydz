#pragma once

#include "../color.hpp"
#include "../light.hpp"
#include "math.hpp"
#include "../mesh3d.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/skybox.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/material/postprocess_material.hpp"
#include "rydz_graphics/pipeline/batches.hpp"
#include "../transform.hpp"
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
  Skybox const* active_skybox{};
  PostProcessDescriptor postprocess{};
  bool active{};
  bool has_postprocess{};
  bool orthographic{};
  float near_plane{0.1F};
  float far_plane{1000.0F};

  auto clear() -> void { *this = ExtractedView{}; }
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

  auto clear() -> void { *this = ExtractedLights{}; }
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
    float distance_to_camera{0.0F};
  };

  std::vector<MaterialItem> materials;
  std::vector<Item> items;
  auto clear() -> void { *this = ExtractedMeshes{}; }
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
  auto clear() -> void { *this = ExtractedUi{}; }
};

} // namespace ecs
