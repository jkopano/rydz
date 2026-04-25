#pragma once

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/components/environment.hpp"
#include "rydz_graphics/components/light.hpp"
#include "rydz_graphics/components/mesh3d.hpp"
#include "rydz_graphics/components/sprite.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/lighting/shadow.hpp"
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
    Res<ExtractedView> view,
    Query<AmbientLight> ambient_query,
    Query<DirectionalLight> dir_query,
    Query<PointLight, GlobalTransform> point_query,
    Query<SpotLight, GlobalTransform> spot_query,
    ResMut<ExtractedLights> lights
  ) -> void {
    lights->clear();

    if (view->active_environment != nullptr) {
      lights->ambient_light = view->active_environment->ambient_light;
      lights->dir_light = view->active_environment->directional_light;
      lights->has_directional =
        lights->dir_light.intensity > 0.0F && lights->dir_light.direction.length_sq() > 1e-8F;
    }

    if (lights->ambient_light.intensity <= 0.0F) {
      for (auto [ambient] : ambient_query.iter()) {
        lights->ambient_light = *ambient;
        break;
      }
    }

    if (!lights->has_directional) {
      for (auto [dir] : dir_query.iter()) {
        lights->dir_light = *dir;
        lights->has_directional = dir->intensity > 0.0F && dir->direction.length_sq() > 1e-8F;
        break;
      }
    }

    if (!view->active) {
      return;
    }

    auto const frustum_planes =
      extract_frustum_planes(view->camera_view.proj * view->camera_view.view);

    for (auto [point_light, global] : point_query.iter()) {
      Vec3 const position = global->translation();
      if (point_light->range <= 0.001F ||
          !sphere_in_frustum(position, point_light->range, frustum_planes)) {
        continue;
      }

      lights->point_lights.push_back(
        ExtractedLights::PointLight{
          .position = position,
          .color = point_light->color,
          .intensity = point_light->intensity,
          .range = point_light->range,
          .casts_shadows = point_light->casts_shadows,
          .screen_space_shadows = false,
          .shadow_slot = -1,
        }
      );
    }

    for (auto [spot_light, global] : spot_query.iter()) {
      Vec3 const position = global->translation();
      if (spot_light->range <= 0.001F ||
          !sphere_in_frustum(position, spot_light->range, frustum_planes)) {
        continue;
      }

      lights->spot_lights.push_back(
        ExtractedLights::SpotLight{
          .position = position,
          .direction = global->forward(),
          .color = spot_light->color,
          .intensity = spot_light->intensity,
          .range = spot_light->range,
          .inner_angle = spot_light->inner_angle,
          .outer_angle = spot_light->outer_angle,
        }
      );
    }
  }

  static auto shadows(
    Res<ExtractedView> view,
    ResMut<ExtractedLights> lights,
    Res<ShadowSettings> settings,
    ResMut<ExtractedShadows> shadows,
    Query<MeshBounds, GlobalTransform, Opt<ComputedVisibility>> shadow_caster_query
  ) -> void {
    shadows->clear();

    if (!view->active) {
      return;
    }

    if (settings->directional_enabled && lights->has_directional &&
        lights->dir_light.casts_shadows) {
      AABox caster_bounds{};
      bool has_caster_bounds = false;

      for (auto [bounds, global, computed_visibility] : shadow_caster_query.iter()) {
        if ((computed_visibility != nullptr) && !computed_visibility->visible) {
          continue;
        }

        AABox const world_bounds = transform_bbox(bounds->bbox, global->matrix);
        for (Vec3 const& corner : world_bounds.corners()) {
          caster_bounds.encapsulate(corner);
        }
        has_caster_bounds = true;
      }

      if (has_caster_bounds) {
        shadows->has_directional = true;
        shadows->cascade_count = 1;

        auto& cascade = shadows->directional_cascades[0];
        cascade.shadow_view = ShadowView::from(
          caster_bounds.corners(),
          lights->dir_light.direction,
          settings->cascade_resolution
        );
        cascade.split_distance = view->far_plane;
      }
    }

    if (!settings->point_shadows_enabled || lights->point_lights.empty()) {
      return;
    }

    std::vector<usize> candidates;
    candidates.reserve(lights->point_lights.size());
    for (usize light_index = 0; light_index < lights->point_lights.size();
         ++light_index) {
      auto& point_light = lights->point_lights[light_index];
      if (!point_light.casts_shadows || point_light.range <= 0.001F) {
        continue;
      }

      if (settings->point_screen_space_shadows_enabled) {
        f32 const projected_radius =
          projected_sphere_radius_pixels(*view, point_light.position, point_light.range);
        if (std::isfinite(projected_radius) &&
            projected_radius <= settings->point_screen_space_threshold_px) {
          point_light.screen_space_shadows = true;
          continue;
        }
      }

      candidates.push_back(light_index);
    }

    std::ranges::sort(candidates, [&](usize lhs, usize rhs) -> bool {
      Vec3 const lhs_offset =
        lights->point_lights[lhs].position - view->camera_view.position;
      Vec3 const rhs_offset =
        lights->point_lights[rhs].position - view->camera_view.position;
      return lhs_offset.length_sq() < rhs_offset.length_sq();
    });

    usize const selected_count = std::min<usize>(
      candidates.size(), static_cast<usize>(settings->max_shadowed_point_lights_clamped())
    );
    shadows->point_shadows.reserve(selected_count);

    for (usize point_slot = 0; point_slot < selected_count; ++point_slot) {
      usize const light_index = candidates[point_slot];
      auto& light = lights->point_lights[light_index];
      light.shadow_slot = static_cast<i32>(point_slot);

      f32 const near_plane = 0.1F;
      f32 const far_plane = std::max(light.range, near_plane + 0.001F);
      shadows->point_shadows.push_back(
        ExtractedShadows::PointShadow{
          .light_index = light_index,
          .position = light.position,
          .near_plane = near_plane,
          .far_plane = far_plane,
          .faces = build_point_shadow_views(light.position, near_plane, far_plane),
        }
      );
    }
  }

  static auto clear_meshes(ResMut<ExtractedMeshes> meshes) -> void { meshes->clear(); }

  template <RenderMaterialAsset M>
  static auto meshes(
    Query<
      Mesh3d,
      MeshBounds,
      GlobalTransform,
      MeshMaterial3d<M>,
      Opt<ComputedVisibility>,
      Opt<ViewVisibility>> query,
    Res<ExtractedView> view,
    Res<Assets<M>> material_assets,
    ResMut<MaterialCache> material_cache,
    ResMut<ExtractedMeshes> meshes
  ) -> void {
    for (auto [mesh3d, bounds, global, material, computed_visibility, visibility] :
         query.iter()) {
      if (!mesh3d->mesh.is_valid() || !material->material.is_valid()) {
        continue;
      }
      if (computed_visibility != nullptr && !computed_visibility->visible) {
        continue;
      }

      auto const* material_asset = material_assets->get(material->material);
      if (material_asset == nullptr) {
        continue;
      }

      CompiledMaterial const& compiled =
        material_cache->get_or_compile(material->material, *material_asset);

      auto [it, inserted] = meshes->material_lookup.try_emplace(material->material.id);
      if (inserted) {
        bool const transparent = compiled.render_method == RenderMethod::Transparent;
        bool const casts_shadows = compiled.casts_shadows;
        it->second = meshes->materials.size();
        meshes->materials.push_back(
          ExtractedMeshes::MaterialItem{
            .key = render_material_key(material->material),
            .material = compiled,
            .transparent = transparent,
            .casts_shadows = casts_shadows,
          }
        );
      }

      auto const& material_item = meshes->materials[it->second];
      Vec3 camera_offset = global->translation() - view->camera_view.position;
      AABox const world_bounds = transform_bbox(bounds->bbox, global->matrix);

      meshes->items.push_back(
        ExtractedMeshes::Item{
          .mesh = mesh3d->mesh,
          .material = material_item.key,
          .material_index = it->second,
          .world_transform = global->matrix,
          .world_bounds = world_bounds,
          .distance_sq_to_camera = camera_offset.length_sq(),
          .visible_in_view = visibility != nullptr ? visibility->visible : true,
        }
      );
    }
  }

  static auto ui(Query<Sprite, Transform> textures, ResMut<ExtractedUi> ui) -> void {
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

  static auto overlay(Query<Sprite, Transform> textures, ResMut<ExtractedUi> overlay)
    -> void {
    ui(textures, overlay);
  }
};
namespace detail {
inline auto prepare_mesh(Handle<Mesh> const& handle, Assets<Mesh>& mesh_assets) -> bool {
  auto* mesh = mesh_assets.get(handle);
  if ((mesh == nullptr) || mesh->vertex_count() <= 0 || mesh->vertex_data() == nullptr) {
    return false;
  }
  if (!mesh->uploaded()) {
    mesh->upload(false);
  }
  return true;
}

inline auto opaque_command_less(
  ExtractedMeshes const& meshes, RenderCommand const& lhs, RenderCommand const& rhs
) -> bool {
  auto const& lhs_material = meshes.materials[lhs.material_index];
  auto const& rhs_material = meshes.materials[rhs.material_index];
  if (lhs_material.material.shader.vertex_path !=
      rhs_material.material.shader.vertex_path) {
    return lhs_material.material.shader.vertex_path <
           rhs_material.material.shader.vertex_path;
  }
  if (lhs_material.material.shader.fragment_path !=
      rhs_material.material.shader.fragment_path) {
    return lhs_material.material.shader.fragment_path <
           rhs_material.material.shader.fragment_path;
  }
  if (lhs_material.key.asset_type != rhs_material.key.asset_type) {
    return lhs_material.key.asset_type.hash_code() <
           rhs_material.key.asset_type.hash_code();
  }
  if (lhs_material.key.id != rhs_material.key.id) {
    return lhs_material.key.id < rhs_material.key.id;
  }
  return lhs.mesh.id < rhs.mesh.id;
}
} // namespace detail

struct Queue {
  static auto opaque(Res<ExtractedMeshes> meshes, ResMut<OpaquePhase> phase) -> void {
    phase->clear();
    std::unordered_map<RenderBatchKey, usize> batch_index;

    for (auto const& item : meshes->items) {
      if (!item.visible_in_view) {
        continue;
      }
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
          .material_index = item.material_index,
          .instances = {math::to_rl(item.world_transform)},
          .world_bounds = item.world_bounds,
          .sort_key = item.distance_sq_to_camera,
        };
        phase->commands.push_back(std::move(cmd));
        batch_index.emplace(key, idx);
      } else {
        phase->commands[it->second].instances.push_back(
          math::to_rl(item.world_transform)
        );
      }
    }

    std::ranges::sort(
      phase->commands, [&](RenderCommand const& lhs, RenderCommand const& rhs) -> bool {
        return detail::opaque_command_less(*meshes, lhs, rhs);
      }
    );
  }

  static auto transparent(Res<ExtractedMeshes> meshes, ResMut<TransparentPhase> phase)
    -> void {
    phase->clear();

    for (auto const& item : meshes->items) {
      if (!item.visible_in_view) {
        continue;
      }
      auto const& material = meshes->materials[item.material_index];
      if (!material.transparent) {
        continue;
      }

      RenderCommand cmd{
        .mesh = item.mesh,
        .material_index = item.material_index,
        .instances = {math::to_rl(item.world_transform)},
        .world_bounds = item.world_bounds,
        .sort_key = item.distance_sq_to_camera,
      };
      phase->commands.push_back(std::move(cmd));
    }

    std::ranges::sort(
      phase->commands, [](RenderCommand const& lhs, RenderCommand const& rhs) -> bool {
        return lhs.sort_key > rhs.sort_key;
      }
    );
  }

  static auto shadow(Res<ExtractedMeshes> meshes, ResMut<ShadowPhase> phase) -> void {
    phase->clear();
    for (auto const& item : meshes->items) {
      auto const& material = meshes->materials[item.material_index];
      if (!material.casts_shadows) {
        continue;
      }
      RenderCommand cmd{
        .mesh = item.mesh,
        .material_index = item.material_index,
        .instances = {math::to_rl(item.world_transform)},
        .world_bounds = item.world_bounds,
        .sort_key = item.distance_sq_to_camera,
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
      phase->items, [](UiPhase::Item const& lhs, UiPhase::Item const& rhs) -> bool {
        return lhs.layer < rhs.layer;
      }
    );
  }
};

struct Prepare {
  static auto prepare_meshes(
    Res<ExtractedMeshes> extracted, ResMut<Assets<Mesh>> mesh_assets, NonSendMarker
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
