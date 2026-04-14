#pragma once

#include "color.hpp"
#include "light.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/frustum.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/skybox.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/material/postprocess_material.hpp"
#include "transform.hpp"
#include "visibility.hpp"
#include <array>
#include <vector>

namespace ecs {

struct Sprite {
  Handle<Texture> handle;
  Color tint = kWhite;
  i32 layer{};
};

struct ExtractedView {
  using T = Resource;
  CameraView camera_view{
      .view = Mat4::sIdentity(),
      .proj = Mat4::sIdentity(),
      .position = Vec3{},
  };
  Color clear_color = ClearColor{}.color;
  const Skybox *active_skybox{};
  PostProcessDescriptor postprocess{};
  bool active{};
  bool has_postprocess{};
  bool orthographic{};
  float near_plane{0.1f};
  float far_plane{1000.0f};

  void reset() { *this = ExtractedView{}; }
};

struct ExtractedLights {
  using T = Resource;

  struct PointLight {
    Vec3 position;
    Color color{kWhite};
    float intensity{0.0F};
    float range{0.0F};
  };

  std::vector<PointLight> point_lights{};
  DirectionalLight dir_light{};
  bool has_directional{};

  void reset() { *this = ExtractedLights{}; }
};

struct ExtractedMeshes {
  using T = Resource;

  struct Inner {
    Handle<Mesh> mesh{};
    CompiledMaterial material{};
    Mat4 world_transform{Mat4::sIdentity()};
    float distance_to_camera{0.0F};
    bool transparent{};
    bool casts_shadows{true};
  };

  std::vector<Inner> items;
  void clear() { items.clear(); }
};

struct ExtractedUiItem {
  Handle<Texture> texture{};
  Transform transform{};
  Color tint = kWhite;
  i32 layer = 0;
};

struct ExtractedUi {
  using T = Resource;

  std::vector<ExtractedUiItem> items{};
  void clear() { items.clear(); }
};

struct RenderExtractSystems {
  static void extract_view_system(
      Query<Camera3DComponent, ActiveCamera, GlobalTransform, Opt<ClearColor>,
            Opt<Skybox>, Opt<PostProcessMaterial>>
          cam_query,
      Res<Window> window, ResMut<ExtractedView> view) {
    view->reset();
    const float aspect = compute_camera_aspect_ratio(
        static_cast<float>(window->width), static_cast<float>(window->height));

    for (auto [cam_comp, _, cam_gt, clear_color, skybox, postprocess] :
         cam_query.iter()) {
      view->camera_view = compute_camera_view(*cam_gt, *cam_comp, aspect);
      if (clear_color != nullptr) {
        view->clear_color = clear_color->color;
      }
      view->active_skybox = skybox;
      view->active = true;
      view->orthographic = cam_comp->is_orthographic();
      view->near_plane = cam_comp->near_plane;
      view->far_plane = cam_comp->far_plane;
      if (postprocess) {
        view->postprocess = postprocess->material;
        view->has_postprocess = true;
      }
      break;
    }
  }

  static void
  extract_lighting_system(Query<DirectionalLight> dir_query,
                          Query<PointLight, GlobalTransform> point_query,
                          ResMut<ExtractedLights> lights) {
    lights->reset();

    for (auto [dir] : dir_query.iter()) {
      lights->dir_light = *dir;
      lights->has_directional = true;
      break;
    }

    for (auto [point_light, global] : point_query.iter()) {
      lights->point_lights.push_back(ExtractedLights::PointLight{
          .position = global->translation(),
          .color = point_light->color,
          .intensity = point_light->intensity,
          .range = point_light->range,
      });
    }
  }

  static void clear_extracted_meshes_system(ResMut<ExtractedMeshes> meshes) {
    meshes->clear();
  }

  static void extract_meshes_system(
      Query<Mesh3d, GlobalTransform, MeshMaterial3d, Opt<ViewVisibility>> query,
      Res<ExtractedView> view, Res<Assets<Material>> material_assets,
      ResMut<ExtractedMeshes> meshes) {
    for (auto [mesh3d, global, material, visibility] : query.iter()) {
      if (!mesh3d->mesh.is_valid() || !material->material.is_valid()) {
        continue;
      }
      if (visibility == nullptr || !visibility->visible) {
        continue;
      }

      const auto *material_asset = material_assets->get(material->material);
      if (material_asset == nullptr) {
        continue;
      }

      CompiledMaterial compiled = material_asset->compiled;
      const bool transparent =
          compiled.render_method == RenderMethod::Transparent;
      const bool casts_shadows = compiled.casts_shadows;
      Vec3 camera_offset = global->translation() - view->camera_view.position;

      meshes->items.push_back(ExtractedMeshes::Inner{
          .mesh = mesh3d->mesh,
          .material = std::move(compiled),
          .world_transform = global->matrix,
          .distance_to_camera = camera_offset.LengthSq(),
          .transparent = transparent,
          .casts_shadows = casts_shadows,
      });
    }
  }

  static void extract_ui_system(Query<Sprite, Transform> textures,
                                ResMut<ExtractedUi> ui) {
    ui->clear();

    for (auto [texture, transform] : textures.iter()) {
      if (!texture->handle.is_valid()) {
        continue;
      }

      ui->items.push_back(ExtractedUiItem{
          .texture = texture->handle,
          .transform = *transform,
          .tint = texture->tint,
          .layer = texture->layer,
      });
    }
  }

  static void extract_overlay_system(Query<Sprite, Transform> textures,
                                     ResMut<ExtractedUi> overlay) {
    extract_ui_system(textures, overlay);
  }
};

} // namespace ecs
