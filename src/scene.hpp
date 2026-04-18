#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

struct MapTag {};
struct RotateMarker {};

struct FreeLook {
  f32 speed_mult = 1.0F;
};
struct Cinematic {
  f32 smooth_factor = 0.5F;
};
struct Locked {
  Entity target;
};

enum Ste { Working, NotWorking };

using CameraState = Variant<FreeLook, Cinematic, Locked>;
using NotCameraState = Variant<Cinematic, Locked>;

struct CameraController {
  using Storage = SparseSetStorage<CameraController>;
  f32 move_speed = 20.0F;
  f32 mouse_sensitivity = 0.003F;
  f32 yaw = -90.0F;
  f32 pitch = -20.0F;
  bool enabled = true;
};

inline auto
camera_controller_system(Query<Mut<Transform>, CameraController> query,
                         Res<Time> time, Res<Input> input) -> void {

  for (auto [t, ctrl] : query.iter()) {
    if (!ctrl->enabled) {
      return;
    }

    f32 dt = time->delta_seconds;
    f32 speed = ctrl->move_speed;

    Vec3 move = Vec3::ZERO;
    Vec3 forward = t->forward();
    Vec3 right = t->right();

    if (input->key_down(KEY_LEFT_CONTROL)) {
      speed *= 3.0F;
    }
    if (input->key_down(KEY_W)) {
      move += forward;
    }
    if (input->key_down(KEY_S)) {
      move -= forward;
    }
    if (input->key_down(KEY_D)) {
      move += right;
    }
    if (input->key_down(KEY_A)) {
      move -= right;
    }
    if (input->key_down(KEY_SPACE)) {
      move += Vec3(0, 1, 0);
    }
    if (input->key_down(KEY_LEFT_SHIFT)) {
      move -= Vec3(0, 1, 0);
    }

    if (move.length_sq() > 0.0F) {
      move = move.normalized();
      t->translation = t->translation + move * (speed * dt);
    }
  }
}

inline void
camera_mouse_system(Query<Mut<CameraController>, Mut<Transform>> query,
                    Res<Input> input) {
  Vec2 m = input->mouse_delta();

  for (auto [ctrl, t] : query.iter()) {
    if (!ctrl->enabled || (m.x == 0 && m.y == 0)) {
      return;
    }

    ctrl->yaw -= m.x * ctrl->mouse_sensitivity * 57.F;
    ctrl->pitch -= m.y * ctrl->mouse_sensitivity * 57.F;

    ctrl->pitch = std::min(ctrl->pitch, 89.0F);
    ctrl->pitch = std::max(ctrl->pitch, -89.0F);

    t->rotation = Quat::from_euler(
        Vec3(ctrl->pitch * DEG2RAD, DEG2RAD * ctrl->yaw, 0.0F));
  }
}

inline auto spawn_map(Cmd cmd, Res<AssetServer> asset_server) -> void {
  cmd.spawn(MapTag{}, CameraState{FreeLook{}},
            SceneRoot{asset_server->load<Scene>("res/models/sponza.glb")},
            Transform{.scale = Vec3{10.1F, 10.1F, 10.1F}});
}

inline auto spawn_model(Cmd cmd, Res<AssetServer> asset_server) -> void {
  cmd.spawn(SceneRoot{asset_server->load<Scene>("res/models/hot_sun.glb")},
            Transform{.scale = Vec3{10.1F, 10.1F, 10.1F}});
}

inline auto some_shit(Query<CameraState> q) -> void {
  if (auto x = q.single(); x) {
    const auto *state = *x;

    std::visit(over{[&](const FreeLook &fl) -> void {
                      // std::print("Kamera w trybie FreeLook\n");
                    },
                    [&](const Cinematic &c) -> void {
                      // std::print("Kamera w trybie Cinematic\n");
                    },
                    [&](const Locked &l) -> void {
                      // std::print("Kamera śledzi encję: {}\n", 10);
                    }},
               *state);
  }
}

inline auto spawn_some_texture(Cmd cmd, ResMut<Assets<ecs::Texture>> textures,
                               NonSendMarker) -> void {
  cmd.spawn(ecs::Sprite{.handle = textures->add(
                            gl::load_texture("res/textures/stone.jpg"))},
            Transform{
                .translation = Vec3(10.0F, 10.0F, 0.0F),
                .scale = Vec3::splat(1.0F),
            });
}

struct LightsSpawned {
  using T = Resource;
  bool done = false;
};

inline void spawn_lights_on_input(Cmd cmd,
                                  ResMut<Assets<ecs::Texture>> textures,
                                  ResMut<Assets<ecs::Mesh>> meshes,
                                  ResMut<Assets<ecs::Material>> materials,
                                  ResMut<LightsSpawned> lights,
                                  Res<Input> input, NonSendMarker) {
  if (lights->done || !input->key_pressed(KEY_L)) {
    return;
  }

  cmd.spawn(DirectionalLight{
      .color = {255, 242, 230, 255},
      .direction = Vec3(-0.3F, -1.0F, -0.5F),
      .intensity = 0.5F,
  });

  auto stone_tex = textures->add(gl::load_texture("res/textures/stone.jpg"));
  auto stone_mat = materials->add(StandardMaterial::from_texture(stone_tex));

  auto cube_h = meshes->add(mesh::cube());

  cmd.spawn(Mesh3d{cube_h},
            PointLight{.color = {255, 0, 0, 255},
                       .intensity = 8800.0F,
                       .range = 2000.0F},
            Transform::from_xyz(-50.0F, 50.0F, 0.0F));

  auto cube_h2 = meshes->add(mesh::cube());

  cmd.spawn(Mesh3d{cube_h2}, MeshMaterial3d{stone_mat},
            Transform::from_xyz(50.0F, 50.0F, 0.0F));

  cmd.spawn(PointLight{.color = {75, 75, 255, 255},
                       .intensity = 800.0F,
                       .range = 200.0F},
            Transform::from_xyz(100.0F, 100.0F, 0.0F));

  lights->done = true;
}

inline auto setup_camera(Cmd cmd, NonSendMarker) -> void {
  cmd.spawn(Camera3DComponent::perspective(), ActiveCamera{},
            Transform::from_xyz(8, 6, 8).look_at(Vec3::ZERO),
            CameraController{},
            PostProcessMaterial{DefaultPostProcessMaterial{}},
            Skybox::from("res/hdri/skybox"));
  rl::DisableCursor();
}

inline void scene_plugin(App &app) {
  app.add_plugin(system_multithreading({true}));
  app.insert_resource(LightsSpawned{});

  app.add_systems(Startup, setup_camera);
  app.add_systems(Startup, spawn_some_texture);
  app.add_systems(Update, group(spawn_map, spawn_model).run_if(run_once()));

  app.add_systems(Update, camera_controller_system);
  app.add_systems(Update, camera_mouse_system);
  app.add_systems(Update, some_shit);

  app.add_systems(Update, spawn_lights_on_input);
}
