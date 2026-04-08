// 09 - Spawn i Despawn
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include "rydz_platform/rydz_platform.hpp"
#include <print>
#include <vector>

using namespace ecs;
using namespace math;

struct BulletTag {};
struct Lifetime {
  f32 remaining = 3.0f;
};

void setup(Cmd cmd, ResMut<Assets<rydz_gl::Mesh>> meshes, NonSendMarker) {
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 15, 20).look_at(Vec3::sZero()));

  cmd.spawn(DirectionalLight{
      .color = WHITE,
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.5f,
  });

  // podłoga
  auto floor_h = meshes->add(mesh::plane(30, 30));
  cmd.spawn(Mesh3d{floor_h},
            MeshMaterial3d<>{StandardMaterial::from_color(DARKGRAY)},
            Transform{});
}

// spawn_batch
// korzystasz jak musisz dużo rzeczy wyspawnic, powinno być szybsze(na razie nie
// jest xd)
void batch_spawn(Cmd cmd, ResMut<Assets<rydz_gl::Mesh>> meshes, Res<Input> input,
                 NonSendMarker) {
  if (!input->key_pressed(KEY_SPACE))
    return;

  auto sphere_h = meshes->add(mesh::sphere(0.3f));

  // budujemy wektor tupli — spawn_batch przyjmuje range
  std::vector<Tuple<BulletTag, Lifetime, Mesh3d, MeshMaterial3d<>, Transform>>
      batch;

  for (i32 x = -3; x <= 3; ++x) {
    for (i32 z = -3; z <= 3; ++z) {
      batch.emplace_back(BulletTag{}, Lifetime{3.0f}, Mesh3d{sphere_h},
                         MeshMaterial3d<>{StandardMaterial::from_color(
                             rl::Color{static_cast<u8>(100 + x * 20),
                                       static_cast<u8>(100 + z * 20), 200,
                                       255})},
                         Transform::from_xyz(x * 1.5f, 5.0f, z * 1.5f));
    }
  }

  cmd.spawn_batch(batch);
  std::println("batch spawned {} entities", batch.size());
}

// despawnik po czasei
void lifetime_system(Query<Entity, Mut<Lifetime>> query, Res<Time> time,
                     Cmd cmd) {
  for (auto [entity, life] : query.iter()) {
    life->remaining -= time->delta_seconds;
    if (life->remaining <= 0.0f) {
      cmd.despawn(entity);
    }
  }
}

void gravity_system(Query<Mut<Transform>, With<BulletTag>> query,
                    Res<Time> time) {
  for (auto [t] : query.iter()) {
    f32 y = t->translation.GetY() - 3.0f * time->delta_seconds;
    if (y < 0.3f)
      y = 0.3f;
    t->translation = Vec3(t->translation.GetX(), y, t->translation.GetZ());
  }
}

int main() {
  App app;
  app.add_plugin(
         window_plugin({1024, 768, "09 - Spawn/Despawn (SPACE/F/X)", 60}))
      .add_plugin(rydz_platform::RayPlugin::install({}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update,
                   group(batch_spawn, lifetime_system, gravity_system))
      .run();
}
