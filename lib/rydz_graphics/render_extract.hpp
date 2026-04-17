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
#include "rydz_graphics/material/standard_material.hpp"
#include "transform.hpp"
#include "visibility.hpp"
#include <array>
#include <concepts>
#include <vector>

namespace ecs {

struct Sprite {
  Handle<Texture> handle;
  Color tint = kWhite;
  i32 layer{};
};

template <typename M>
concept IsExtracted = requires(const M &m) {
  { m.clear() } -> std::same_as<void>;
};

struct ExtractedView {
  using T = Resource;
  CameraView camera_view{
      .view = Mat4::IDENTITY,
      .proj = Mat4::IDENTITY,
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

  auto clear() -> void { *this = ExtractedView{}; }
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

  auto clear() -> void { *this = ExtractedLights{}; }
};

struct ExtractedMeshes {
  using T = Resource;

  struct Item {
    Handle<Mesh> mesh{};
    CompiledMaterial material{};
    Mat4 world_transform{Mat4::IDENTITY};
    float distance_to_camera{0.0F};
    bool transparent{};
    bool casts_shadows{true};
  };

  std::vector<Item> items;
  auto clear() -> void { *this = ExtractedMeshes{}; }
};

struct ExtractedUi {
  using T = Resource;

  struct Item {
    Handle<Texture> texture{};
    Transform transform{};
    Color tint = kWhite;
    i32 layer = 0;
  };

  std::vector<Item> items{};
  auto clear() -> void { *this = ExtractedUi{}; }
};

struct Extract {
  static void
  view(Query<Camera3DComponent, ActiveCamera, GlobalTransform, Opt<ClearColor>,
             Opt<gl::Skybox>, Opt<PostProcessMaterial>>
           cam_query,
       Res<Window> window, ResMut<ExtractedView> view) {
    view->clear();
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
      if (postprocess != nullptr) {
        view->postprocess = postprocess->material;
        view->has_postprocess = true;
      }
      break;
    }
  }

  static auto lighting(Query<DirectionalLight> dir_query,
                       Query<PointLight, GlobalTransform> point_query,
                       ResMut<ExtractedLights> lights) -> void {
    lights->clear();

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

  static auto clear_meshes(ResMut<ExtractedMeshes> meshes) -> void {
    meshes->clear();
  }

  static auto meshes(
      Query<Mesh3d, GlobalTransform, MeshMaterial3d, Opt<ViewVisibility>> query,
      Res<ExtractedView> view, Res<Assets<Material>> material_assets,
      ResMut<ExtractedMeshes> meshes) -> void {
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

      meshes->items.push_back(ExtractedMeshes::Item{
          .mesh = mesh3d->mesh,
          .material = std::move(compiled),
          .world_transform = global->matrix,
          .distance_to_camera = camera_offset.length_sq(),
          .transparent = transparent,
          .casts_shadows = casts_shadows,
      });
    }
  }

  static auto ui(Query<Sprite, Transform> textures, ResMut<ExtractedUi> ui)
      -> void {
    ui->clear();

    for (auto [texture, transform] : textures.iter()) {
      if (!texture->handle.is_valid()) {
        continue;
      }

      ui->items.push_back(ExtractedUi::Item{
          .texture = texture->handle,
          .transform = *transform,
          .tint = texture->tint,
          .layer = texture->layer,
      });
    }
  }

  static auto overlay(Query<Sprite, Transform> textures,
                      ResMut<ExtractedUi> overlay) -> void {
    ui(textures, overlay);
  }
};

struct Queue {
  template <typename M, typename P>
  static auto queue(Res<M> meshes, ResMut<P> phase) -> void {
    phase->queue(*meshes);
  };
};

} // namespace ecs
