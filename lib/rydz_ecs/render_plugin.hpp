#pragma once
#include "app.hpp"
#include "asset.hpp"
#include "camera3d.hpp"
#include "light.hpp"
#include "material3d.hpp"
#include "mesh3d.hpp"
#include "transform.hpp"
#include "visibility.hpp"
#include <raylib-cpp/raylib-cpp.hpp>

namespace ecs {

struct ClearColor {
  Color color = {30, 30, 40, 255};
};

inline void apply_materials_system(World &world) {
  auto *mesh_storage = world.get_vec_storage<Mesh3d>();
  auto *mat_storage = world.get_vec_storage<Material3d>();
  auto *model_assets = world.get_resource<Assets<Model>>();
  auto *tex_assets = world.get_resource<Assets<Texture2D>>();
  if (!mesh_storage || !mat_storage || !model_assets)
    return;

  auto entities = mat_storage->entity_indices();
  for (auto e : entities) {
    auto *mesh_comp = mesh_storage->get(e);
    auto *mat_comp = mat_storage->get(e);
    if (!mesh_comp || !mat_comp)
      continue;

    Model *model = model_assets->get(mesh_comp->model);
    if (!model)
      continue;

    mat_comp->material.apply(*model, tex_assets);
  }
}

inline void render_3d_system(World &world) {
  auto *clear_color = world.get_resource<ClearColor>();
  Color bg = clear_color ? clear_color->color : Color{30, 30, 40, 255};

  auto *model_assets = world.get_resource<Assets<Model>>();
  if (!model_assets)
    return;

  Camera3D cam = {};
  cam.position = {10, 10, 10};
  cam.target = {0, 0, 0};
  cam.up = {0, 1, 0};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  auto *cam_storage = world.get_vec_storage<Camera3DComponent>();
  auto *active_storage = world.get_vec_storage<ActiveCamera>();
  auto *transform_store = world.get_vec_storage<Transform3D>();

  if (cam_storage && active_storage && transform_store) {
    auto cam_entities = active_storage->entity_indices();
    if (!cam_entities.empty()) {
      Entity cam_entity = cam_entities[0];
      auto *cam_comp = cam_storage->get(cam_entity);
      auto *cam_transform = transform_store->get(cam_entity);
      if (cam_comp && cam_transform) {
        cam = build_camera(cam_transform->translation, cam_transform->forward(),
                           cam_transform->up(), *cam_comp);
      }
    }
  }

  BeginDrawing();
  ClearBackground(bg);
  BeginMode3D(cam);

  DrawGrid(20, 1.0f);

  auto *mesh_storage = world.get_vec_storage<Mesh3d>();
  auto *global_storage = world.get_vec_storage<GlobalTransform>();
  auto *vis_storage = world.get_vec_storage<ComputedVisibility>();

  if (mesh_storage && global_storage) {
    auto entities = mesh_storage->entity_indices();
    for (auto e : entities) {
      auto *mesh3d = mesh_storage->get(e);
      if (!mesh3d || !mesh3d->model.is_valid())
        continue;

      if (vis_storage) {
        auto *vis = vis_storage->get(e);
        if (vis && !vis->visible)
          continue;
      }

      auto *global = global_storage->get(e);
      if (!global)
        continue;

      Model *model = model_assets->get(mesh3d->model);
      if (!model)
        continue;

      model->transform = global->matrix;
      DrawModel(*model, {0, 0, 0}, 1.0f, WHITE);
    }
  }

  EndMode3D();

  DrawFPS(10, 10);
  EndDrawing();
}

inline void render_plugin(App &app) {
  if (!app.world().contains_resource<Assets<Model>>()) {
    app.insert_resource(Assets<Model>{});
  }
  if (!app.world().contains_resource<Assets<Texture2D>>()) {
    app.insert_resource(Assets<Texture2D>{});
  }
  if (!app.world().contains_resource<ClearColor>()) {
    app.insert_resource(ClearColor{});
  }
  if (!app.world().contains_resource<AssetServer>()) {
    app.insert_resource(AssetServer{});
  }

  app.add_systems(ScheduleLabel::PostUpdate,
                  [](World &w) { propagate_transforms(w); });

  app.add_systems(ScheduleLabel::PostUpdate,
                  [](World &w) { compute_visibility(w); });

  app.add_systems(ScheduleLabel::PreUpdate,
                  [](World &w) { apply_materials_system(w); });

  app.add_systems(ScheduleLabel::Last, [](World &w) { render_3d_system(w); });
}

} // namespace ecs
