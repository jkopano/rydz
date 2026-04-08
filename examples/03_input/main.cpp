// 03 - Input
// Pokazuje: input_plugin, Res<Input>, key_pressed/key_down, mouse_delta

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include "rydz_platform/rydz_platform.hpp"
#include <print>

using namespace ecs;
using namespace math;

struct Player {
  f32 speed = 200.0f;
};

void setup(Cmd cmd, ResMut<Assets<ecs::Mesh>> meshes,
           ResMut<Assets<ecs::Material>> materials, NonSendMarker) {
  // kamera (transformy by trzeba rozwinąć kiedyś tam)
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 30, 0).look_at(Vec3::sZero()));

  // ładowanko mesha
  auto cube_mesh = meshes->add(mesh::cube(1, 1, 1));
  auto cube_mat = materials->add(StandardMaterial::from_color(
      {255, 255, 255, 255}));

  cmd.spawn(Player{20.0f}, Transform::from_xyz(0, 0.5f, 0),
            Mesh3d{cube_mesh}, MeshMaterial3d{cube_mat});
}

void player_movement(Query<Mut<Transform>, Player> query, Res<Input> input,
                     Res<Time> time) {
  for (auto [t, player] : query.iter()) {
    f32 dt = time->delta_seconds;
    f32 speed = player->speed;

    Vec3 move = Vec3::sZero();
    if (input->key_down(KEY_W))
      move += Vec3(0, 0, -1);
    if (input->key_down(KEY_S))
      move += Vec3(0, 0, 1);
    if (input->key_down(KEY_A))
      move += Vec3(-1, 0, 0);
    if (input->key_down(KEY_D))
      move += Vec3(1, 0, 0);

    if (move.LengthSq() > 0.0f) {
      move = move.Normalized();
      t->translation = t->translation + move * (speed * dt);
    }

    // key_pressed = jedno naciśnięcie w sensie nie hold
    if (input->key_pressed(KEY_R)) {
      t->translation = Vec3::sZero();
      std::println("reset");
    }
  }
}

void print_mouse(Res<Input> input) {
  Vec2 md = input->mouse_delta();
  if (md.x != 0 || md.y != 0) {
    std::println("mouse delta: ({:.1f}, {:.1f})", md.x, md.y);
  }
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
          .window = {800, 600, "03 - Input", 60},
      }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, player_movement)
      .add_systems(ScheduleLabel::Update, print_mouse)
      .run();
}
