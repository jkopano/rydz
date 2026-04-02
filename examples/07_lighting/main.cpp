// 07 - Lighting
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"

using namespace ecs;
using namespace math;

struct OrbitLight {
  f32 radius;
  f32 speed;
  f32 phase;
};

void setup(Cmd cmd, ResMut<Assets<rl::Model>> models, NonSendMarker) {
  // kamera
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 8, 15).look_at(Vec3::sZero()));

  // podłoga
  auto floor = rl::LoadModelFromMesh(mesh::plane(30, 30));
  auto floor_h = models->add(std::move(floor));
  cmd.spawn(Model3d{floor_h},
            Material3d{StandardMaterial::from_color({200, 200, 200, 255})},
            Transform{});

  // kilka obiektów na scenie
  auto sphere = rl::LoadModelFromMesh(mesh::sphere(1.0f));
  auto sphere_h = models->add(std::move(sphere));
  for (int i = -2; i <= 2; ++i) {
    cmd.spawn(Model3d{sphere_h},
              Material3d{StandardMaterial::from_color(WHITE)},
              Transform::from_xyz(i * 4.0f, 1.0f, 0.0f));
  }

  // światło kierunkowe
  cmd.spawn(DirectionalLight{
      .color = {255, 240, 220, 255},
      .direction = Vec3(-0.5f, -1.0f, -0.3f),
      .intensity = 0.3f,
  });

  auto make_orbit_light = [&](rl::Color color, f32 radius, f32 speed, f32 phase,
                              f32 y, f32 intensity) {
    auto h = models->add(rl::LoadModelFromMesh(mesh::cube(0.3f, 0.3f, 0.3f)));
    cmd.spawn(
        Model3d{h}, Material3d{StandardMaterial::from_color(color)},
        PointLight{.color = color, .intensity = intensity, .range = 30.0f},
        Transform::from_xyz(0, y, 0), OrbitLight{radius, speed, phase});
  };

  make_orbit_light({255, 50, 50, 255}, 6.0f, 1.5f, 0.0f, 3.0f, 20.0f);
  make_orbit_light({50, 50, 255, 255}, 8.0f, 1.0f, 2.1f, 4.0f, 20.0f);
  make_orbit_light({50, 255, 50, 255}, 5.0f, 2.0f, 4.2f, 2.5f, 20.0f);
}

// obitujsz swiatlami
void orbit_system(Query<Mut<Transform>, OrbitLight> query, Res<Time> time) {
  f32 t = time->elapsed_seconds;
  for (auto [tx, orbit] : query.iter()) {
    f32 angle = t * orbit->speed + orbit->phase;
    tx->translation = Vec3(cosf(angle) * orbit->radius, tx->translation.GetY(),
                           sinf(angle) * orbit->radius);
  }
}

int main() {
  App app;
  app.add_plugin(window_plugin({1024, 768, "07 - Lighting", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .insert_resource(ClearColor{{15, 15, 25, 255}})
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, orbit_system)
      .run();
}
