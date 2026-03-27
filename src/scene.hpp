#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

struct MapTag {};
struct RotateMarker {};

struct CameraController {
  using Storage = SparseSetStorage<CameraController>;
  f32 move_speed = 20.0f;
  f32 mouse_sensitivity = 0.003f;
  f32 yaw = -90.0f;
  f32 pitch = -20.0f;
  bool enabled = true;
};

inline void
camera_controller_system(Query<Mut<Transform>, CameraController> query,
                         Res<Time> time, Res<Input> input) {

  for (auto [t, ctrl] : query.iter()) {
    if (!ctrl->enabled)
      return;

    f32 dt = time->delta_seconds;
    f32 speed = ctrl->move_speed;

    Vec3 move = Vec3::sZero();
    Vec3 forward = t->forward();
    Vec3 right = t->right();

    if (input->key_down(KEY_LEFT_CONTROL))
      speed *= 3.0f;
    if (input->key_down(KEY_W))
      move += forward;
    if (input->key_down(KEY_S))
      move -= forward;
    if (input->key_down(KEY_D))
      move += right;
    if (input->key_down(KEY_A))
      move -= right;
    if (input->key_down(KEY_SPACE))
      move += Vec3(0, 1, 0);
    if (input->key_down(KEY_LEFT_SHIFT))
      move -= Vec3(0, 1, 0);

    if (move.LengthSq() > 0.0f) {
      move = move.Normalized();
      t->translation = t->translation + move * (speed * dt);
    }
  }
}

inline void
camera_mouse_system(Query<Mut<CameraController>, Mut<Transform>> query,
                    Res<Input> input) {
  Vector2 m = input->mouse_delta();

  for (auto [ctrl, t] : query.iter()) {
    if (!ctrl->enabled || (m.x == 0 && m.y == 0))
      return;

    ctrl->yaw -= m.x * ctrl->mouse_sensitivity * 57.f;
    ctrl->pitch -= m.y * ctrl->mouse_sensitivity * 57.f;

    if (ctrl->pitch > 89.0f)
      ctrl->pitch = 89.0f;
    if (ctrl->pitch < -89.0f)
      ctrl->pitch = -89.0f;

    t->rotation = Quat::sEulerAngles(
        Vec3(ctrl->pitch * DEG2RAD, ctrl->yaw * DEG2RAD, 0.0f));
  }
}

inline void spawn_map(Cmd cmd, Res<AssetServer> asset_server) {
  cmd.spawn(MapTag{},
            Model3d{asset_server->load<rl::Model>("res/models/race_map.glb")},
            Transform{.scale = Vec3{0.1f, 0.1f, 0.1f}});
}

inline void spawn_some_texture(Cmd cmd, ResMut<Assets<rl::Texture2D>> textures,
                               NonSendMarker) {
  auto stone_tex = textures->add(rl::LoadTexture("res/textures/stone.jpg"));
  cmd.spawn(ecs::Texture{stone_tex},
            Transform{
                .translation = Vec3(10.0f, 10.0f, 0.0f),
                .scale = Vec3::sReplicate(1.0f),
            });
}

struct LightsSpawned {
  using Type = Resource;
  bool done = false;
};

inline void spawn_lights_on_input(Cmd cmd, ResMut<Assets<rl::Model>> models,
                                  ResMut<Assets<rl::Texture2D>> textures,
                                  ResMut<Assets<rl::Mesh>> meshes,
                                  ResMut<LightsSpawned> lights,
                                  Res<Input> input, NonSendMarker) {
  if (lights->done || !input->key_pressed(KEY_L))
    return;

  cmd.spawn(DirectionalLight{
      .color = {255, 242, 230, 255},
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.5f,
  });

  auto stone_tex = textures->add(rl::LoadTexture("res/textures/stone.jpg"));

  rl::Mesh cube_mesh = mesh::cube();
  rl::Model cube_model = rl::LoadModelFromMesh(cube_mesh);
  auto cube_h = models->add(std::move(cube_model));

  cmd.spawn(Model3d{cube_h},
            PointLight{.color = {255, 0, 0, 255},
                       .intensity = 8800.0f,
                       .range = 2000.0f},
            Transform::from_xyz(-50.0f, 50.0f, 0.0f));

  rl::Model cube_model2 = rl::LoadModelFromMesh(mesh::cube());
  auto cube_h2 = models->add(std::move(cube_model2));

  cmd.spawn(Model3d{cube_h2},
            Material3d{StandardMaterial::from_texture(stone_tex)},
            Transform::from_xyz(50.0f, 50.0f, 0.0f));

  cmd.spawn(PointLight{.color = {75, 75, 255, 255},
                       .intensity = 800.0f,
                       .range = 200.0f},
            Transform::from_xyz(100.0f, 100.0f, 0.0f));

  lights->done = true;
}

inline void setup_camera(Cmd cmd, NonSendMarker) {
  cmd.spawn(Camera3DComponent::orthographic(), ActiveCamera{},
            Transform::from_xyz(8, 6, 8).look_at(Vec3::sZero()),
            CameraController{} // Skybox::from("res/hdri/skybox")
  );
  rl::DisableCursor();
}

inline void scene_plugin(App &app) {
  app.add_plugin(input_plugin);
  app.add_plugin(system_multithreading({true}));
  app.insert_resource(LightsSpawned{});

  app.add_systems(ScheduleLabel::Startup, setup_camera);
  app.add_systems(ScheduleLabel::Startup, spawn_some_texture);
  app.add_systems(ScheduleLabel::Update, group(spawn_map).run_if(run_once()));

  app.add_systems(ScheduleLabel::Update, camera_controller_system);
  app.add_systems(ScheduleLabel::Update, camera_mouse_system);

  app.add_systems(ScheduleLabel::Update, spawn_lights_on_input);
}
