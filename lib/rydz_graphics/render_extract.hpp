#pragma once
#include "clear_color.hpp"
#include "light.hpp"
#include "material3d.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "render_config.hpp"
#include "rl.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/frustum.hpp"
#include "skybox.hpp"
#include "transform.hpp"
#include "visibility.hpp"
#include <array>
#include <vector>

namespace ecs {

struct Texture {
  Handle<rl::Texture2D> handle;
  rl::Color tint = WHITE;
  i32 layer = 0;
};

struct ExtractedView {
  using T = Resource;
  CameraView camera_view{
      .view = Mat4::sIdentity(),
      .proj = Mat4::sIdentity(),
      .position = Vec3(10, 10, 10),
  };
  rl::Color clear_color = ClearColor{}.color;
  const Skybox *active_skybox = nullptr;
  RenderConfig render_config{};
  bool active = false;
  bool has_render_config = false;
  bool orthographic = false;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;

  void reset() {
    camera_view = {
        .view = Mat4::sIdentity(),
        .proj = Mat4::sIdentity(),
        .position = Vec3(10, 10, 10),
    };
    clear_color = ClearColor{}.color;
    active_skybox = nullptr;
    render_config = {};
    active = false;
    has_render_config = false;
    orthographic = false;
    near_plane = 0.1f;
    far_plane = 1000.0f;
  }
};

struct ExtractedPointLight {
  rl::Vector3 position = {0, 0, 0};
  rl::Color color = WHITE;
  float intensity = 0.0f;
  float range = 0.0f;
};

struct ExtractedLights {
  using T = Resource;

  DirectionalLight dir_light{};
  bool has_directional = false;
  std::vector<ExtractedPointLight> point_lights;

  void reset() {
    dir_light = {};
    has_directional = false;
    point_lights.clear();
  }
};

struct ExtractedMesh {
  Handle<rl::Mesh> mesh{};
  MaterialDescriptor material{};
  Mat4 world_transform = Mat4::sIdentity();
  float distance_to_camera = 0.0f;
  bool transparent = false;
  bool casts_shadows = true;
};

struct ExtractedMeshes {
  using T = Resource;
  std::vector<ExtractedMesh> items;

  void clear() { items.clear(); }
};

struct ExtractedUiItem {
  Handle<rl::Texture2D> texture{};
  Transform transform{};
  rl::Color tint = WHITE;
  i32 layer = 0;
};

struct ExtractedUi {
  using T = Resource;
  std::vector<ExtractedUiItem> items;

  void clear() { items.clear(); }
};

inline rl::Vector3 color_to_vec3(rl::Color color) {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
}

struct RenderExtractSystems {
  static void
  extract_view_system(Query<Camera3DComponent, ActiveCamera, GlobalTransform,
                            Opt<ClearColor>, Opt<Skybox>, Opt<RenderConfig>>
                          cam_query,
                      ResMut<ExtractedView> view) {
    view->reset();

    for (auto [cam_comp, _, cam_gt, clear_color, skybox, render_config] :
         cam_query.iter()) {
      if (!cam_comp || !cam_gt) {
        continue;
      }

      view->camera_view = compute_camera_view(*cam_gt, *cam_comp);
      if (clear_color) {
        view->clear_color = clear_color->color;
      }
      view->active_skybox = skybox;
      view->active = true;
      view->orthographic = cam_comp->is_orthographic();
      view->near_plane = cam_comp->near_plane;
      view->far_plane = cam_comp->far_plane;
      if (render_config) {
        view->render_config = *render_config;
        view->has_render_config = true;
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
      if (!dir) {
        continue;
      }
      lights->dir_light = *dir;
      lights->has_directional = true;
      break;
    }

    for (auto [point_light, global] : point_query.iter()) {
      if (!point_light || !global) {
        continue;
      }

      lights->point_lights.push_back(ExtractedPointLight{
          .position = to_rl(global->translation()),
          .color = point_light->color,
          .intensity = point_light->intensity,
          .range = point_light->range,
      });
    }
  }

  static void clear_extracted_meshes_system(ResMut<ExtractedMeshes> meshes) {
    meshes->clear();
  }

  template <IsMaterial M>
  static void extract_meshes_system(
      Query<Mesh3d, GlobalTransform, MeshMaterial3d<M>, Opt<ViewVisibility>>
          query,
      Res<ExtractedView> view, ResMut<ExtractedMeshes> meshes) {
    for (auto [mesh3d, global, material, visibility] : query.iter()) {
      if (!mesh3d || !global || !material || !mesh3d->mesh.is_valid()) {
        continue;
      }
      if (visibility && !visibility->visible) {
        continue;
      }

      MaterialDescriptor descriptor = material->material.describe();
      const bool transparent = detail::material_is_transparent(descriptor);
      const bool casts_shadows = descriptor.flags.casts_shadows;
      Vec3 camera_offset = global->translation() - view->camera_view.position;

      meshes->items.push_back(ExtractedMesh{
          .mesh = mesh3d->mesh,
          .material = std::move(descriptor),
          .world_transform = global->matrix,
          .distance_to_camera = camera_offset.LengthSq(),
          .transparent = transparent,
          .casts_shadows = casts_shadows,
      });
    }
  }

  static void extract_ui_system(Query<Texture, Transform> textures,
                                ResMut<ExtractedUi> ui) {
    ui->clear();

    for (auto [texture, transform] : textures.iter()) {
      if (!texture || !transform || !texture->handle.is_valid()) {
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

  static void extract_overlay_system(Query<Texture, Transform> textures,
                                     ResMut<ExtractedUi> overlay) {
    extract_ui_system(textures, overlay);
  }

private:
  struct detail {
    static bool material_is_transparent(const MaterialDescriptor &material) {
      return material.flags.transparent;
    }
  };
};

} // namespace ecs
