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

void setup(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes, NonSendMarker) {
  // kamera
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 8, 15).look_at(Vec3::sZero()));

  // podłoga
  auto floor_h = meshes->add(mesh::plane(30, 30));
  cmd.spawn(Mesh3d{floor_h},
            Material3d{StandardMaterial::from_color({200, 200, 200, 255})},
            Transform{});

  // kilka obiektów na scenie
  auto sphere_h = meshes->add(mesh::sphere(1.0f));
  for (int i = -2; i <= 2; ++i) {
    cmd.spawn(Mesh3d{sphere_h}, Material3d{StandardMaterial::from_color(WHITE)},
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
    auto h = meshes->add(mesh::cube(0.3f, 0.3f, 0.3f));
    cmd.spawn(
        Mesh3d{h}, Material3d{StandardMaterial::from_color(color)},
        PointLight{.color = color, .intensity = intensity, .range = 30.0f},
        Transform::from_xyz(0, y, 0), OrbitLight{radius, speed, phase});
  };

  constexpr int kLightCount = 64;
  const rl::Color palette[] = {
      {255, 90, 90, 255},  {90, 140, 255, 255},  {100, 255, 140, 255},
      {255, 210, 90, 255}, {255, 120, 220, 255}, {120, 255, 255, 255},
  };

  for (int i = 0; i < kLightCount; ++i) {
    rl::Color color = palette[i % (sizeof(palette) / sizeof(palette[0]))];
    f32 radius = 4.5f + static_cast<f32>(i % 6) * 1.2f;
    f32 speed = 0.5f + static_cast<f32>(i % 8) * 0.18f;
    f32 phase = static_cast<f32>(i) * 0.47f;
    f32 y = 1.5f + static_cast<f32>(i % 5) * 0.8f;
    f32 intensity = 10.0f + static_cast<f32>(i % 4) * 4.0f;
    make_orbit_light(color, radius, speed, phase, y, intensity);
  }
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
      .add_plugin(render_plugin)
      .insert_resource(ClearColor{{15, 15, 25, 255}})
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, orbit_system)
      .run();
}
