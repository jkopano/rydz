#pragma once
#include "rydz_ecs/rydz_ecs.hpp"

using namespace ecs;

struct MapTag {};
struct HouseTag {};
struct CarTag {};
struct RotateMarker {};

struct CameraController {
  float move_speed = 20.0f;
  float mouse_sensitivity = 0.003f;
  float yaw = -90.0f;
  float pitch = -20.0f;
  bool enabled = true;
};

inline void
camera_controller_system(Query<Mut<Transform3D>, CameraController> query,
                         Res<Time> time, NonSendMarker) {
  query.for_each([&](Transform3D *t, const CameraController *ctrl) {
    if (!ctrl->enabled)
      return;

    float dt = time->delta_seconds;
    float speed = ctrl->move_speed;

    if (IsKeyDown(KEY_LEFT_CONTROL))
      speed *= 3.0f;

    Vector3 forward = t->forward();
    Vector3 right = t->right();

    Vector3 move = {0, 0, 0};
    if (IsKeyDown(KEY_W))
      move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S))
      move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D))
      move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A))
      move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_SPACE))
      move = Vector3Add(move, {0, 1, 0});
    if (IsKeyDown(KEY_LEFT_SHIFT))
      move = Vector3Subtract(move, {0, 1, 0});

    if (Vector3LengthSqr(move) > 0.0f) {
      move = Vector3Normalize(move);
      t->translation =
          Vector3Add(t->translation, Vector3Scale(move, speed * dt));
    }
  });
}

inline void camera_mouse_system(World &world, NonSendMarker) {
  auto *ctrl_storage = world.get_vec_storage<CameraController>();
  auto *transform_storage = world.get_vec_storage<Transform3D>();
  if (!ctrl_storage || !transform_storage)
    return;

  Vector2 mouse_delta = GetMouseDelta();
  if (mouse_delta.x == 0 && mouse_delta.y == 0)
    return;

  for (auto e : ctrl_storage->entity_indices()) {
    auto *ctrl = const_cast<CameraController *>(ctrl_storage->get(e));
    auto *t = transform_storage->get(e);
    if (!ctrl || !t || !ctrl->enabled)
      continue;

    ctrl->yaw -= mouse_delta.x * ctrl->mouse_sensitivity * 57.2958f;
    ctrl->pitch -= mouse_delta.y * ctrl->mouse_sensitivity * 57.2958f;

    if (ctrl->pitch > 89.0f)
      ctrl->pitch = 89.0f;
    if (ctrl->pitch < -89.0f)
      ctrl->pitch = -89.0f;

    t->rotation =
        QuaternionFromEuler(ctrl->pitch * DEG2RAD, ctrl->yaw * DEG2RAD, 0.0f);
  }
}

inline void toggle_camera_system(World &world, NonSendMarker) {
  if (!IsKeyPressed(KEY_ESCAPE))
    return;

  auto *ctrl_storage = world.get_vec_storage<CameraController>();
  if (!ctrl_storage)
    return;

  for (auto e : ctrl_storage->entity_indices()) {
    auto *ctrl = const_cast<CameraController *>(ctrl_storage->get(e));
    if (!ctrl)
      continue;
    ctrl->enabled = !ctrl->enabled;

    if (ctrl->enabled)
      DisableCursor();
    else
      EnableCursor();
  }
}

inline void spawn_map(Cmd cmd, ResMut<Assets<Model>> model_assets,
                      NonSendMarker) {
  Model map_model = LoadModel("res/models/race_map.glb");
  auto map_h = model_assets->add(std::move(map_model));

  cmd.spawn(MapTag{}, Mesh3d{map_h},
            Transform3D{
                .translation = {700.0f, 1.0f, 700.0f},
                .scale = {0.03f, 0.03f, 0.03f},
            });
}

struct HouseHandles {
  Handle<Model> house;
  bool loaded = false;
};

inline void spawn_houses_on_input(Cmd cmd, ResMut<Assets<Model>> model_assets,
                                  ResMut<HouseHandles> handles, NonSendMarker) {
  if (!IsKeyPressed(KEY_H))
    return;

  if (!handles->loaded) {
    Model house_model = LoadModel("res/models/old_house.glb");
    handles->house = model_assets->add(std::move(house_model));
    handles->loaded = true;
  }

  float spacing = 20.0f;
  float scale = 0.015f;
  int grid = 100;

  for (int x = -grid; x < grid; ++x) {
    for (int z = -grid; z < grid; ++z) {
      cmd.spawn(HouseTag{}, Mesh3d{handles->house},
                Transform3D{
                    .translation = {x * spacing, 0.0f, z * spacing},
                    .scale = {scale, scale, scale},
                });
    }
  }
}

struct CarHandles {
  Handle<Model> car;
  bool loaded = false;
  bool spawned = false;
};

void spawn_car_on_input(Cmd cmd, ResMut<Assets<Model>> model_assets,
                        ResMut<CarHandles> car_handles, NonSendMarker) {
  if (car_handles->spawned)
    return;
  if (!IsKeyPressed(KEY_G))
    return;

  if (!car_handles->loaded) {
    Model car_model = LoadModel("res/models/first-car.glb");
    car_handles->car = model_assets->add(std::move(car_model));
    car_handles->loaded = true;
  }

  cmd.spawn(CarTag{}, Mesh3d{car_handles->car},
            Transform3D{
                .translation = {10.0f, 1.7f, 10.0f},
                .scale = {3.0f, 3.0f, 3.0f},
            });

  car_handles->spawned = true;
}

struct LightsSpawned {
  bool done = false;
};

void spawn_lights_on_input(Cmd cmd, ResMut<LightsSpawned> lights,
                           NonSendMarker) {
  if (lights->done || !IsKeyPressed(KEY_L))
    return;

  cmd.spawn(DirectionalLight{
      .color = {255, 242, 230, 255},
      .direction = {-0.3f, -1.0f, -0.5f},
      .intensity = 0.8f,
  });

  cmd.spawn(PointLight{.color = {255, 230, 150, 255},
                       .intensity = 5000.0f,
                       .range = 150.0f},
            Transform3D::from_xyz(0.0f, 30.0f, 80.0f));

  cmd.spawn(PointLight{.color = {255, 75, 75, 255},
                       .intensity = 800.0f,
                       .range = 200.0f},
            Transform3D::from_xyz(-50.0f, 50.0f, 0.0f));

  cmd.spawn(PointLight{.color = {75, 75, 255, 255},
                       .intensity = 800.0f,
                       .range = 200.0f},
            Transform3D::from_xyz(50.0f, 50.0f, 0.0f));

  lights->done = true;
}

void rotate_marked(Query<Mut<Transform3D>, RotateMarker> query,
                   Res<Time> time) {
  query.for_each([&](Transform3D *t, const RotateMarker *) {
    Quaternion rot = QuaternionFromEuler(0, time->delta_seconds * 0.5f, 0);
    t->rotation = QuaternionMultiply(t->rotation, rot);
  });
}

void setup_camera(Cmd cmd, NonSendMarker) {
  cmd.spawn(Camera3DComponent{60.0f}, ActiveCamera{},
            Transform3D::from_xyz(8, 6, 8).look_at({0, 0, 0}),
            CameraController{});
  DisableCursor();
}
inline void scene_plugin(App &app) {
  app.insert_resource(HouseHandles{});
  app.insert_resource(CarHandles{});
  app.insert_resource(LightsSpawned{});

  app.add_systems(ScheduleLabel::Startup, setup_camera);
  app.add_systems(ScheduleLabel::Startup,
                  into_system(spawn_map).run_if(run_once()));

  app.add_systems(ScheduleLabel::Update, camera_controller_system);
  app.add_systems(ScheduleLabel::Update, camera_mouse_system);
  app.add_systems(ScheduleLabel::Update, toggle_camera_system);

  app.add_systems(ScheduleLabel::Update, spawn_houses_on_input);
  app.add_systems(ScheduleLabel::Update, spawn_car_on_input);
  app.add_systems(ScheduleLabel::Update, spawn_lights_on_input);

  app.add_systems(ScheduleLabel::Update, rotate_marked);
}
