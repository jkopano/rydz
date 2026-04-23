#pragma once

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/components/environment.hpp"
#include "rydz_graphics/components/light.hpp"
#include "rydz_graphics/components/mesh3d.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/material/postprocess_material.hpp"
#include "rydz_graphics/material/standard_material.hpp"
#include "rydz_graphics/pipeline/batches.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_graphics/spatial/frustum.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include "rydz_graphics/spatial/visibility.hpp"
#include <array>
#include <concepts>
#include <unordered_map>
#include <vector>

namespace ecs {

struct Sprite {
  Handle<Texture> handle;
  Color tint = Color::WHITE;
  i32 layer{};
};

template <typename M>
concept IsExtracted = requires(M const& m) {
  { m.clear() } -> std::same_as<void>;
};

struct Extract {
  static auto view(
    Query<
      Camera3d,
      ActiveCamera,
      GlobalTransform,
      Opt<ClearColor>,
      Opt<Environment>,
      Opt<PostProcessMaterial>> cam_query,
    Res<Window> window,
    ResMut<ExtractedView> view
  ) -> void {
    view->clear();
    view->viewport = gl::Rectangle{
      0.0F,
      0.0F,
      static_cast<float>(window->width),
      static_cast<float>(window->height),
    };
    float const aspect =
      compute_camera_aspect_ratio(view->viewport.width, view->viewport.height);

    for (auto [cam_comp, _, cam_gt, clear_color, environment, postprocess] :
         cam_query.iter()) {
      view->camera_view = compute_camera_view(*cam_gt, *cam_comp, aspect);
      if (environment != nullptr) {
        view->active_environment = environment;
        view->clear_color = environment->clear_color;
      } else if (clear_color != nullptr) {
        view->clear_color = clear_color->color;
      }
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

  static auto lighting(
    Query<DirectionalLight> dir_query,
    Query<PointLight, GlobalTransform> point_query,
    ResMut<ExtractedLights> lights
  ) -> void {
    lights->clear();

    for (auto [dir] : dir_query.iter()) {
      lights->dir_light = *dir;
      lights->has_directional = true;
      break;
    }

    for (auto [point_light, global] : point_query.iter()) {
      lights->point_lights.push_back(
        ExtractedLights::PointLight{
          .position = global->translation(),
          .color = point_light->color,
          .intensity = point_light->intensity,
          .range = point_light->range,
        }
      );
    }
  }

  static auto clear_meshes(ResMut<ExtractedMeshes> meshes) -> void {
    meshes->clear();
  }

  template <RenderMaterialAsset M>
  static auto meshes(
    Query<Mesh3d, GlobalTransform, MeshMaterial3d<M>, Opt<ViewVisibility>>
      query,
    Res<ExtractedView> view,
    Res<Assets<M>> material_assets,
    ResMut<ExtractedMeshes> meshes
  ) -> void {
    std::unordered_map<u32, usize> material_cache;

    for (auto [mesh3d, global, material, visibility] : query.iter()) {
      if (!mesh3d->mesh.is_valid() || !material->material.is_valid()) {
        continue;
      }
      if (visibility == nullptr || !visibility->visible) {
        continue;
      }

      auto const* material_asset = material_assets->get(material->material);
      if (material_asset == nullptr) {
        continue;
      }

      auto [compiled_iter, inserted] =
        material_cache.try_emplace(material->material.id);
      if (inserted) {
        CompiledMaterial compiled =
          compile_render_material_asset(*material_asset);
        bool const transparent =
          compiled.render_method == RenderMethod::Transparent;
        bool const casts_shadows = compiled.casts_shadows;
        compiled_iter->second = meshes->materials.size();
        meshes->materials.push_back(
          ExtractedMeshes::MaterialItem{
            .key = render_material_key(material->material),
            .material = std::move(compiled),
            .transparent = transparent,
            .casts_shadows = casts_shadows,
          }
        );
      }

      auto const& material_item = meshes->materials[compiled_iter->second];
      Vec3 camera_offset = global->translation() - view->camera_view.position;

      meshes->items.push_back(
        ExtractedMeshes::Item{
          .mesh = mesh3d->mesh,
          .material = material_item.key,
          .material_index = compiled_iter->second,
          .world_transform = global->matrix,
          .distance_to_camera = camera_offset.length_sq(),
        }
      );
    }
  }

  static auto ui(Query<Sprite, Transform> textures, ResMut<ExtractedUi> ui)
    -> void {
    ui->clear();

    for (auto [texture, transform] : textures.iter()) {
      if (!texture->handle.is_valid()) {
        continue;
      }

      ui->items.push_back(
        ExtractedUi::Item{
          .texture = texture->handle,
          .transform = *transform,
          .tint = texture->tint,
          .layer = texture->layer,
        }
      );
    }
  }

  static auto overlay(
    Query<Sprite, Transform> textures, ResMut<ExtractedUi> overlay
  ) -> void {
    ui(textures, overlay);
  }
};
namespace detail {
inline auto prepare_mesh(Handle<Mesh> const& handle, Assets<Mesh>& mesh_assets)
  -> bool {
  auto* mesh = mesh_assets.get(handle);
  if ((mesh == nullptr) || mesh->vertex_count() <= 0 ||
      mesh->vertex_data() == nullptr) {
    return false;
  }
  if (!mesh->uploaded()) {
    mesh->upload(false);
  }
  return true;
}
} // namespace detail

struct Queue {
  static auto opaque(Res<ExtractedMeshes> meshes, ResMut<OpaquePhase> phase)
    -> void {
    phase->clear();
    std::unordered_map<RenderBatchKey, usize> batch_index;

    for (auto const& item : meshes->items) {
      auto const& material = meshes->materials[item.material_index];
      if (material.transparent) {
        continue;
      }

      RenderBatchKey key{.mesh = item.mesh, .material = item.material};

      auto it = batch_index.find(key);
      if (it == batch_index.end()) {
        usize const idx = phase->commands.size();
        RenderCommand cmd{
          .mesh = item.mesh,
          .material = material.material,
          .instances = {math::to_rl(item.world_transform)},
          .sort_key = item.distance_to_camera,
        };
        phase->commands.push_back(std::move(cmd));
        batch_index.emplace(key, idx);
      } else {
        phase->commands[it->second].instances.push_back(
          math::to_rl(item.world_transform)
        );
      }
    }
  }

  static auto transparent(
    Res<ExtractedMeshes> meshes, ResMut<TransparentPhase> phase
  ) -> void {
    phase->clear();

    for (auto const& item : meshes->items) {
      auto const& material = meshes->materials[item.material_index];
      if (!material.transparent) {
        continue;
      }

      RenderCommand cmd{
        .mesh = item.mesh,
        .material = material.material,
        .instances = {math::to_rl(item.world_transform)},
        .sort_key = item.distance_to_camera,
      };
      phase->commands.push_back(std::move(cmd));
    }

    std::ranges::sort(
      phase->commands,
      [](RenderCommand const& lhs, RenderCommand const& rhs) -> bool {
        return lhs.sort_key > rhs.sort_key;
      }
    );
  }

  static auto shadow(Res<ExtractedMeshes> meshes, ResMut<ShadowPhase> phase)
    -> void {
    phase->clear();
    for (auto const& item : meshes->items) {
      auto const& material = meshes->materials[item.material_index];
      if (!material.casts_shadows) {
        continue;
      }
      RenderCommand cmd{
        .mesh = item.mesh,
        .material = material.material,
        .instances = {math::to_rl(item.world_transform)},
        .sort_key = item.distance_to_camera,
      };
      phase->commands.push_back(std::move(cmd));
    }
  }

  static auto ui(Res<ExtractedUi> ui, ResMut<UiPhase> phase) -> void {
    phase->clear();
    for (auto const& item : ui->items) {
      phase->items.push_back(
        UiPhase::Item{
          .texture = item.texture,
          .transform = item.transform,
          .tint = item.tint,
          .layer = item.layer,
        }
      );
    }

    std::ranges::stable_sort(
      phase->items, [](UiPhase::Item const& lhs, UiPhase::Item const& rhs) {
        return lhs.layer < rhs.layer;
      }
    );
  }
};

struct Prepare {
  static auto prepare_meshes(
    Res<ExtractedMeshes> extracted,
    ResMut<Assets<Mesh>> mesh_assets,
    NonSendMarker
  ) -> void {
    std::unordered_set<u32> seen;

    for (auto const& item : extracted->items) {
      if (!seen.insert(item.mesh.id).second) {
        continue;
      }

      detail::prepare_mesh(item.mesh, *mesh_assets);
    }
  }
};

} // namespace ecs
