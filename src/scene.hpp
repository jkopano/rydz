#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include <print>

using namespace ecs;
using namespace math;

struct MapTag {};
struct HouseTag {};
struct CarTag {};
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
camera_controller_system(Query<Mut<Transform3D>, CameraController> query,
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
camera_mouse_system(Query<Mut<CameraController>, Mut<Transform3D>> query,
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
  cmd.spawn(
      MapTag{},
      Model3d{asset_server->load<rl::Model>("res/models/sponza_atrium_3.glb")},
      Transform3D{.scale = Vec3{10.1f, 10.1f, 10.1f}});
}

struct HouseHandles {
  using Type = ResourceType;
  Handle<rl::Model> house;
  bool loaded = false;
};

inline void spawn_houses_on_input(Cmd cmd, Res<AssetServer> asset_server,
                                  ResMut<HouseHandles> handles,
                                  Res<Input> input) {
  if (handles->loaded)
    return;
  if (!input->key_pressed(KEY_H))
    return;
  handles->loaded = true;

  f32 spacing = 20.0f;
  f32 scale = 0.015f;
  i32 grid = 100;

  for (i32 x = -grid; x < grid; ++x) {
    for (i32 z = -grid; z < grid; ++z) {
      cmd.spawn(
          HouseTag{},
          Model3d{asset_server->load<rl::Model>("res/models/old_house.glb")},
          Transform3D{
              .translation = Vec3(x * spacing, 0.0f, z * spacing),
              .scale = Vec3::sReplicate(scale),
          });
    }
  }
}

struct CarHandles {
  using Type = ResourceType;
  Handle<rl::Model> car;
  bool loaded = false;
  bool spawned = false;
};

inline void spawn_car_on_input(Cmd cmd, Res<AssetServer> asset_server,
                               ResMut<CarHandles> car_handles,
                               Res<Input> input) {
  if (car_handles->spawned)
    return;
  if (!input->key_pressed(KEY_G))
    return;

  if (!car_handles->loaded) {
    car_handles->car =
        asset_server->load<rl::Model>("res/models/stylized_planet.glb");
    car_handles->loaded = true;
  }

  cmd.spawn(CarTag{}, Model3d{car_handles->car},
            Transform3D{
                .translation = Vec3(100.0f, 10.7f, 10.0f),
                .scale = Vec3::sReplicate(3.f),
            });

  car_handles->spawned = true;
}

struct LightsSpawned {
  using Type = ResourceType;
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
            Transform3D::from_xyz(-50.0f, 50.0f, 0.0f));

  rl::Model cube_model2 = rl::LoadModelFromMesh(mesh::cube());
  auto cube_h2 = models->add(std::move(cube_model2));

  cmd.spawn(Model3d{cube_h2},
            Material3d{StandardMaterial::from_texture(stone_tex)},
            Transform3D::from_xyz(50.0f, 50.0f, 0.0f));

  cmd.spawn(PointLight{.color = {75, 75, 255, 255},
                       .intensity = 800.0f,
                       .range = 200.0f},
            Transform3D::from_xyz(100.0f, 100.0f, 0.0f));

  lights->done = true;
}

inline void setup_camera(Cmd cmd, NonSendMarker) {
  cmd.spawn(Camera3DComponent{60.0}, ActiveCamera{},
            Transform3D::from_xyz(8, 6, 8).look_at(Vec3::sZero()),
            CameraController{}, Skybox::from("res/hdri/skybox"));
  rl::DisableCursor();
}

struct Health {
  u32 value = 100;
};
struct Position {
  i32 x = 10;
  i32 y = 20;
};
struct Damage {
  u32 value = 100;
};

inline auto player_bundle(Health health, Position position, Damage damage) {
  struct PlayerBundle {
    using Type = BundleType;
    Position position;
    Health health;
    Damage damage;
  };

  return PlayerBundle{.position = position, .health = health, .damage = damage};
}

inline auto spawn_player(Cmd cmd) {
  cmd.spawn(player_bundle({100}, {100, 100}, {100}));
};

inline auto print_player(Query<Health, Position, Damage> query) {
  for (auto [h, p, d] : query.iter()) {
    std::println("Player: {} {} {}", h->value, p->x, p->y);
  }
}

inline void scene_plugin(App &app) {
  app.add_plugin(input_plugin);
  app.insert_resource(HouseHandles{});
  app.insert_resource(CarHandles{});
  app.insert_resource(LightsSpawned{});

  app.add_systems(ScheduleLabel::Startup, setup_camera);
  app.add_systems(ScheduleLabel::Startup, spawn_player);
  app.add_systems(ScheduleLabel::Update,
                  group(print_player).run_if(run_once()));
  app.add_systems(ScheduleLabel::Update, group(spawn_map).run_if(run_once()));

  app.add_systems(ScheduleLabel::Update, camera_controller_system);
  app.add_systems(ScheduleLabel::Update, camera_mouse_system);

  app.add_systems(ScheduleLabel::Update, spawn_houses_on_input);
  app.add_systems(ScheduleLabel::Update, spawn_car_on_input);
  app.add_systems(ScheduleLabel::Update, spawn_lights_on_input);
}
