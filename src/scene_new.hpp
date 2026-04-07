#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_camera/rydz_camera.hpp"
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_ecs/core/input.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include "rydz_ui/ui_plugin.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

// ── Components ──────────────────────────────────────────────────────────────

struct Player {
  using Storage = SparseSetStorage<Player>;
  f32 move_speed = 8.0f;
};

// Isometric camera offset from the player
static const float kCamOffX = 10.0f;
static const float kCamOffY = 10.0f;
static const float kCamOffZ = 10.0f;

// ── Systems ──────────────────────────────────────────────────────────────────

// Move the player using WSAD relative to the isometric view direction
inline void player_movement_system(Query<Mut<Transform>, Player> query,
                                   Res<Time> time, Res<Input> input) {
  // Isometric forward/right vectors (top-down, ignoring Y)
  const Vec3 iso_forward = Vec3(-1.0f, 0.0f, -1.0f).Normalized(); // W
  const Vec3 iso_right = Vec3(1.0f, 0.0f, -1.0f).Normalized();    // D

  for (auto [t, player] : query.iter()) {
    f32 dt = time->delta_seconds;
    f32 speed = player->move_speed;

    Vec3 move = Vec3::sZero();

    if (input->key_down(KEY_W))
      move += iso_forward;
    if (input->key_down(KEY_S))
      move -= iso_forward;
    if (input->key_down(KEY_D))
      move += iso_right;
    if (input->key_down(KEY_A))
      move -= iso_right;

    if (move.LengthSq() > 0.0f) {
      move = move.Normalized();
      t->translation = t->translation + move * (speed * dt);
    }
  }
}

// Update isometric camera target to follow the player
inline void
update_isometric_camera_target_system(Query<Mut<IsometricCamera>> cam_query,
                                      Query<Transform, Player> player_query) {

  Vec3 player_pos = Vec3::sZero();
  bool found = false;
  for (auto [pt, _] : player_query.iter()) {
    player_pos = pt->translation;
    found = true;
    break;
  }
  if (!found)
    return;

  for (auto [cam] : cam_query.iter()) {
    cam->target = player_pos;
  }
}

// ── Startup systems ──────────────────────────────────────────────────────────

inline void setup_camera(Cmd cmd, NonSendMarker) {
  cmd.spawn(IsometricCameraBundle::setup(
      Vec3::sZero(), Vec3(kCamOffX, kCamOffY, kCamOffZ), 20.0f, 12.0f));
}

inline void setup_lighting(Cmd cmd, NonSendMarker,
                           ResMut<Assets<rl::Mesh>> meshes) {
  cmd.spawn(AmbientLight{
      .color = {60, 60, 70, 255},
      .intensity = 0.35f,
  });

  cmd.spawn(DirectionalLight{
      .color = {255, 244, 220, 255},
      .direction = Vec3(-0.6f, -1.0f, -0.4f).Normalized(),
      .intensity = 0.9f,
  });

  cmd.spawn(
      PointLight{.color = {0, 255, 0, 255}, .intensity = 90.f, .range = 600.0f},
      Transform::from_xyz(0.0f, 3.0f, 0.0f)

      // Mesh3d{meshes->add(mesh::cube(0.5f, 0.5f,
      // 0.5f)))}
  );
}

inline void spawn_ground(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes,
                         ResMut<Assets<rl::Texture2D>> textures,
                         NonSendMarker) {
  auto plane_h = meshes->add(mesh::plane(20.0f, 20.0f, 1, 1));

  cmd.spawn(Mesh3d{plane_h},
            // MeshMaterial3d<>{
            //     StandardMaterial::from_color({80, 160, 80, 255})},
            MeshMaterial3d<>{StandardMaterial::from_texture(
                textures->add(rl::LoadTexture("res/textures/brick.png")))},
            Transform{});
}

inline void spawn_player(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes,
                         ResMut<Assets<rl::Texture2D>> textures,
                         NonSendMarker) {
  auto cube_h = meshes->add(mesh::cube(1.0f, 1.0f, 1.0f));

  cmd.spawn(Mesh3d{cube_h},
            MeshMaterial3d<>{StandardMaterial::from_texture(
                textures->add(rl::LoadTexture("res/textures/stone.jpg")))},
            Transform::from_xyz(0.0f, 0.5f, 0.0f), Player{});
}

// UI
void setup_ui(Res<rydz::ui::UiRoot> root, Cmd cmd) {
  if (!root.ptr) {
    return;
  }

  cmd.entity(root->root)
      .insert(rydz::ui::Style{
          .direction = rydz::ui::Direction::Row,
          .align = rydz::ui::Align::Start,
          .justify = rydz::ui::Justify::End,
          .padding = rydz::ui::UiRect{10, 10, 10, 10},
      });

  Entity panel = cmd.spawn(rydz::ui::UiNode{},
                           rydz::ui::Panel{rl::Color{200, 60, 60, 255}},
                           rydz::ui::Style{
                               .direction = rydz::ui::Direction::Column,
                               .padding = rydz::ui::UiRect{10, 10, 10, 10},
                               .size = {rydz::ui::SizeValue::px(300.0f),
                                        rydz::ui::SizeValue::px(120.0f)},
                           },
                           Parent{root->root})
                     .id();

  cmd.spawn(rydz::ui::UiNode{},
            rydz::ui::Label{.text = "Hello UI", .font_size = 18.0f},
            rydz::ui::Style{}, Parent{panel});
}

// ── Plugin ───────────────────────────────────────────────────────────────────

inline void scene_plugin(App &app) {
  app.add_plugin(Input::install);
  app.add_plugin(UiPlugin::install);
  app.add_plugin(system_multithreading({true}));
  app.add_plugin(engine::scripting_plugin);
  app.add_plugin(engine::console_plugin);

  app.add_systems(ScheduleLabel::Startup, setup_camera);
  app.add_systems(ScheduleLabel::Startup, setup_lighting);
  app.add_systems(ScheduleLabel::Startup, spawn_ground);
  app.add_systems(ScheduleLabel::Startup, spawn_player);

  app.add_systems(ScheduleLabel::Startup, setup_ui);

  app.add_systems(ScheduleLabel::Update, player_movement_system);
  app.add_systems(ScheduleLabel::Update, update_isometric_camera_target_system);
  app.add_systems(ScheduleLabel::Update, isometric_camera_system);
}
