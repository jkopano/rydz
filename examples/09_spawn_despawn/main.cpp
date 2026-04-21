// 09 - Spawn i Despawn
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/plugin.hpp"
#include "rydz_platform/mod.hpp"
#include <print>
#include <vector>

using namespace ecs;
using namespace math;

struct BulletTag {};
struct Lifetime {
  f32 remaining = 3.0f;
};

void setup(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  cmd.spawn(
    Camera3d::perspective(60.0f),
    ActiveCamera{},
    Transform::from_xyz(0, 15, 20).look_at(Vec3::ZERO)
  );

  cmd.spawn(
    DirectionalLight{
      .color = ecs::Color::WHITE,
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.5f,
    }
  );

  // podłoga
  auto floor_h = meshes->add(mesh::plane(30, 30));
  auto floor_mat =
    materials->add(StandardMaterial::from_color(ecs::Color::DARKGRAY));
  cmd.spawn(Mesh3d{floor_h}, MeshMaterial3d{floor_mat}, Transform{});
}

// spawn_batch
// korzystasz jak musisz dużo rzeczy wyspawnic, powinno być szybsze(na razie nie
// jest xd)
void batch_spawn(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Material>> materials,
  Res<Input> input,
  NonSendMarker
) {
  if (!input->key_pressed(KEY_SPACE))
    return;

  auto sphere_h = meshes->add(mesh::sphere(0.3f));

  // budujemy wektor tupli — spawn_batch przyjmuje range
  std::vector<Tuple<BulletTag, Lifetime, Mesh3d, MeshMaterial3d<>, Transform>>
    batch;

  for (i32 x = -3; x <= 3; ++x) {
    for (i32 z = -3; z <= 3; ++z) {
      auto material = materials->add(
        StandardMaterial::from_color(
          ecs::Color{
            static_cast<u8>(100 + x * 20),
            static_cast<u8>(100 + z * 20),
            200,
            255
          }
        )
      );
      batch.emplace_back(
        BulletTag{},
        Lifetime{3.0f},
        Mesh3d{sphere_h},
        MeshMaterial3d{material},
        Transform::from_xyz(x * 1.5f, 5.0f, z * 1.5f)
      );
    }
  }

  cmd.spawn_batch(batch);
  std::println("batch spawned {} entities", batch.size());
}

// despawnik po czasei
void lifetime_system(
  Query<Entity, Mut<Lifetime>> query, Res<Time> time, Cmd cmd
) {
  for (auto [entity, life] : query.iter()) {
    life->remaining -= time->delta_seconds;
    if (life->remaining <= 0.0f) {
      cmd.despawn(entity);
    }
  }
}

void gravity_system(
  Query<Mut<Transform>, With<BulletTag>> query, Res<Time> time
) {
  for (auto [t] : query.iter()) {
    f32 y = t->translation.y - 3.0f * time->delta_seconds;
    if (y < 0.3f)
      y = 0.3f;
    t->translation = Vec3(t->translation.x, y, t->translation.z);
  }
}

int main() {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin::install({
        .window = {1024, 768, "09 - Spawn/Despawn (SPACE/F/X)", 60},
      })
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin::install)
    .add_plugin(Input::install)
    .add_systems(Startup, setup)
    .add_systems(Update, group(batch_spawn, lifetime_system, gravity_system))
    .run();
}
