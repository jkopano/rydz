#pragma once

#include "clear_color.hpp"
#include "light.hpp"
#include "material3d.hpp"
#include "math.hpp"
#include "mesh3d.hpp"
#include "postprocess_material.hpp"
#include "render_config.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_gl/core.hpp"
#include "rydz_graphics/frustum.hpp"
#include "skybox.hpp"
#include "transform.hpp"
#include "visibility.hpp"
#include <array>
#include <vector>

namespace ecs {

struct Texture {
  Handle<rydz_gl::Texture> handle;
  rydz_gl::Color tint = rydz_gl::kWhite;
  i32 layer = 0;
};

struct ExtractedView {
  using T = Resource;
  CameraView camera_view{
      .view = Mat4::sIdentity(),
      .proj = Mat4::sIdentity(),
      .position = Vec3(10, 10, 10),
  };
  rydz_gl::Color clear_color = ClearColor{}.color;
  const Skybox *active_skybox = nullptr;
  RenderConfig render_config{};
  PostProcessDescriptor postprocess{};
  bool active = false;
  bool has_render_config = false;
  bool has_postprocess = false;
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
    postprocess = {};
    active = false;
    has_render_config = false;
    has_postprocess = false;
    orthographic = false;
    near_plane = 0.1f;
    far_plane = 1000.0f;
  }
};

struct ExtractedPointLight {
  Vec3 position = Vec3::sZero();
  rydz_gl::Color color = rydz_gl::kWhite;
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
  Handle<rydz_gl::Mesh> mesh{};
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
  Handle<rydz_gl::Texture> texture{};
  Transform transform{};
  rydz_gl::Color tint = rydz_gl::kWhite;
  i32 layer = 0;
};

struct ExtractedUi {
  using T = Resource;
  std::vector<ExtractedUiItem> items;

  void clear() { items.clear(); }
};

inline rydz_gl::Vec3 color_to_vec3(rydz_gl::Color color) {
  return rydz_gl::color_to_vec3(color);
}

struct RenderExtractSystems {
  static void
  extract_view_system(Query<Camera3DComponent, ActiveCamera, GlobalTransform,
                            Opt<ClearColor>, Opt<Skybox>, Opt<RenderConfig>,
                            Opt<PostProcessMaterial>>
                          cam_query,
                      Res<Window> window, ResMut<ExtractedView> view) {
    view->reset();
    const float aspect = compute_camera_aspect_ratio(
        static_cast<float>(window->width), static_cast<float>(window->height));

    for (auto [cam_comp, _, cam_gt, clear_color, skybox, render_config,
               postprocess] :
         cam_query.iter()) {
      if (!cam_comp || !cam_gt) {
        continue;
      }

      view->camera_view = compute_camera_view(*cam_gt, *cam_comp, aspect);
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
