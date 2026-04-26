#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_camera/mod.hpp"
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_levelLoader/rydz_levelLoader.hpp"
#include "rydz_ui/mod.hpp"
#include <algorithm>
#include <print>

using namespace ecs;
using namespace math;

// ── Components ──────────────────────────────────────────────────────────────

struct Player {
  using Storage = SparseSetStorage<Player>;
  f32 move_speed = 8.0f;
};

struct UiMarker {};
struct WasdKeyMarker {
  int keycode;
};

// Isometric camera offset from the player
static float const kCamOffX = 10.0f;
static float const kCamOffY = 10.0f;
static float const kCamOffZ = 10.0f;

// ── Systems ──────────────────────────────────────────────────────────────────

// Run condition - Only run gameplay systems when console is closed

inline bool is_gameplay_active(Res<engine::ConsoleState> console) {
  return !console->is_open;
}

// Move the player using WSAD relative to the isometric view direction
inline void player_movement_system(
  Query<Mut<Transform>, Player> query, Res<Time> time, Res<Input> input
) {
  // Isometric forward/right vectors (top-down, ignoring Y)
  Vec3 const iso_forward = Vec3(-1.0f, 0.0f, -1.0f).normalized(); // W
  Vec3 const iso_right = Vec3(1.0f, 0.0f, -1.0f).normalized();    // D

  for (auto [t, player] : query.iter()) {
    f32 dt = time->delta_seconds;
    f32 speed = player->move_speed;

    Vec3 move = Vec3::ZERO;

    if (input->key_down(KEY_W))
      move += iso_forward;
    if (input->key_down(KEY_S))
      move -= iso_forward;
    if (input->key_down(KEY_D))
      move += iso_right;
    if (input->key_down(KEY_A))
      move -= iso_right;

    if (move.length_sq() > 0.0f) {
      move = move.normalized();
      t->translation = t->translation + move * (speed * dt);
    }
  }
}

// Update isometric camera target to follow the player
inline void update_isometric_camera_target_system(
  Query<Mut<IsometricCamera>> cam_query, Query<Transform, Player> player_query
) {

  Vec3 player_pos = Vec3::ZERO;
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
  cmd.spawn(
    IsometricCameraBundle::setup(
      Vec3::ZERO, Vec3(kCamOffX, kCamOffY, kCamOffZ), 20.0f, 12.0f
    )
  );
}

inline void setup_lighting(
  Cmd cmd, NonSendMarker, ResMut<Assets<ecs::Mesh>> meshes
) {
  cmd.spawn(
    AmbientLight{
      .color = {60, 60, 70, 255},
      .intensity = 0.35f,
    }
  );

  cmd.spawn(
    DirectionalLight{
      .color = {255, 244, 220, 255},
      .direction = Vec3(-0.6f, -1.0f, -0.4f).normalized(),
      .intensity = 0.9f,
    }
  );

  cmd.spawn(
    PointLight{.color = {0, 255, 0, 255}, .intensity = 90.f, .range = 600.0f},
    Transform::from_xyz(0.0f, 3.0f, 0.0f)

    // Mesh3d{meshes->add(mesh::cube(0.5f, 0.5f,
    // 0.5f)))}
  );
}

inline void spawn_ground(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Texture>> textures,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  auto plane_h = meshes->add(mesh::plane(20.0f, 20.0f, 1, 1));
  auto plane_mat = materials->add(
    StandardMaterial::from_texture(
      textures->add(gl::load_texture("res/textures/brick.png"))
    )
  );

  cmd.spawn(Mesh3d{plane_h}, MeshMaterial3d{plane_mat}, Transform{});
}

inline void spawn_player(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Texture>> textures,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  auto cube_h = meshes->add(mesh::cube(1.0f, 1.0f, 1.0f));
  auto cube_mat = materials->add(
    StandardMaterial::from_texture(
      textures->add(gl::load_texture("res/textures/stone.jpg"))
    )
  );

  cmd.spawn(
    Mesh3d{cube_h},
    MeshMaterial3d{cube_mat},
    Transform::from_xyz(0.0f, 0.5f, 0.0f),
    Player{}
  );
}

void setup_ui(Res<rydz::ui::UiRoot> root, Cmd cmd) {
  if (!root.ptr)
    return;

  cmd.entity(root->root)
    .insert(
      rydz::ui::Style{
        .direction = rydz::ui::Direction::Row,
        .align = rydz::ui::Align::Start,
        .justify = rydz::ui::Justify::End,
        .padding =
          rydz::ui::UiRect{.left = 10, .top = 10, .right = 10, .bottom = 10},
      }
    );

  Entity info_panel =
    cmd
      .spawn(
        rydz::ui::UiNode{},
        rydz::ui::Panel{rlColor{.r = 200, .g = 60, .b = 60, .a = 128}},
        rydz::ui::Style{
          .direction = rydz::ui::Direction::Column,
          .padding =
            rydz::ui::UiRect{.left = 10, .top = 10, .right = 10, .bottom = 10},
          .margin =
            rydz::ui::UiRect{.left = 0, .top = 0, .right = 10, .bottom = 0},
          .size =
            {.width = rydz::ui::SizeValue::px(200.0f),
             .height = rydz::ui::SizeValue::px(80.0f)},
        },
        Parent{root->root}
      )
      .id();

  cmd.spawn(
    rydz::ui::UiNode{},
    rydz::ui::Label{.text = "Player Status", .font_size = 14.0f},
    rydz::ui::Style{},
    Parent{info_panel}
  );
  cmd.spawn(
    rydz::ui::UiNode{},
    rydz::ui::Label{.text = "", .font_size = 18.0f},
    rydz::ui::Style{},
    Parent{info_panel},
    UiMarker{}
  );

  Entity wasd_container =
    cmd
      .spawn(
        rydz::ui::UiNode{},
        rydz::ui::Panel{rlColor{0, 0, 0, 160}},
        rydz::ui::Style{
          .direction = rydz::ui::Direction::Row,
          .align = rydz::ui::Align::Center,
          .justify = rydz::ui::Justify::Center,
          .padding =
            rydz::ui::UiRect{.left = 5, .top = 5, .right = 5, .bottom = 5},
          .size =
            {.width = rydz::ui::SizeValue::px(210.0f),
             .height = rydz::ui::SizeValue::px(60.0F)},
        },
        Parent{root->root}
      )
      .id();

  auto spawn_key = [&](int keycode, std::string const& label_text) {
    Entity key_box =
      cmd
        .spawn(
          rydz::ui::UiNode{},
          rydz::ui::Panel{rlColor{.r = 80, .g = 80, .b = 80, .a = 255}},
          rydz::ui::Style{
            .direction = rydz::ui::Direction::Row,
            .align = rydz::ui::Align::Center,
            .justify = rydz::ui::Justify::Center,
            .margin =
              rydz::ui::UiRect{.left = 4, .top = 4, .right = 4, .bottom = 4},
            .size =
              {.width = rydz::ui::SizeValue::px(40.0f),
               .height = rydz::ui::SizeValue::px(40.0f)},
          },
          Parent{wasd_container},
          WasdKeyMarker{keycode}
        )
        .id();

    cmd.spawn(
      rydz::ui::UiNode{},
      rydz::ui::Label{
        .text = label_text,
        .font_size = 20.0f,
        .color = rlColor{.r = 255, .g = 255, .b = 255, .a = 255}
      },
      rydz::ui::Style{},
      Parent{key_box}
    );
  };

  spawn_key(KEY_W, "W");
  spawn_key(KEY_A, "A");
  spawn_key(KEY_S, "S");
  spawn_key(KEY_D, "D");
}

void show_player_position_ui(
  Res<rydz::ui::UiRoot> root,
  Cmd cmd,
  Query<Transform, Player> player_query,
  Query<UiMarker, Mut<rydz::ui::Label>> panel_query
) {

  Vec3 player_pos = Vec3::ZERO;
  for (auto [pt, _] : player_query.iter()) {
    player_pos = pt->translation;
    break;
  }
  std::string player_pos_string = std::format(
    "Pos: {:.2f}, {:.2f}, {:.2f}", player_pos.x, player_pos.y, player_pos.z
  );

  auto result = panel_query.single();
  if (!result)
    return;
  auto [_, panel] = *result;
  panel->text = player_pos_string;
}

void update_wasd_ui_system(
  Query<Mut<rydz::ui::Panel>, WasdKeyMarker> query, Res<Input> input
) {
  // Definiujemy kolory dla stanów przycisku
  auto color_pressed =
    rlColor{.r = 255, .g = 255, .b = 255, .a = 255}; // Jasny biały
  auto color_released =
    rlColor{.r = 128, .g = 128, .b = 128, .a = 128}; // Półprzezroczysty szary

  for (auto [panel, marker] : query.iter()) {
    if (input->key_down(marker->keycode)) {
      panel->background_color = color_pressed;
    } else {
      panel->background_color = color_released;
    }
  }
}

// ── Plugin ───────────────────────────────────────────────────────────────────

inline void scene_plugin(App& app) {
  app.add_plugin(Input::install);
  app.add_plugin(UiPlugin::install);
  app.add_plugin(system_multithreading({true}));
  app.add_plugin(engine::scripting_plugin);
  app.add_plugin(engine::console_plugin);
  app.add_plugin(camera_plugin);

  app.add_systems(ScheduleLabel::Startup, setup_camera);
  app.add_systems(ScheduleLabel::Startup, setup_lighting);
  // app.add_systems(ScheduleLabel::Startup, spawn_ground);
  app.add_systems(ScheduleLabel::Startup, spawn_player);

  app.add_systems(ScheduleLabel::Startup, spawn_model);
  app.add_systems(ScheduleLabel::Startup, spawn_entity_models);

  app.add_systems(ScheduleLabel::Startup, setup_ui);
  app.add_systems(ScheduleLabel::Update, show_player_position_ui);
  app.add_systems(ScheduleLabel::Update, update_wasd_ui_system);

  app.add_systems(
    ScheduleLabel::Update,
    ecs::group(player_movement_system, update_isometric_camera_target_system)
      .run_if(is_gameplay_active)
  );

  app.add_systems(
    RenderPassSet::Cleanup,
    group(engine::ConsoleRenderSystem).before(FramePass::end)
  );
}
