// 13 - Physics
// Demonstrates: PhysicsPlugin, RigidBody, Collider, falling cube on static
// ground

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/components/light.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/plugin.hpp"
#include "rydz_physics/mod.hpp"

#include "rydz_camera/camera3d.hpp"
#include "rydz_platform/mod.hpp"

using namespace ecs;
using namespace physics;
using namespace rydz_math;

struct MovingLight {
  f32 radius;
  f32 speed;
  f32 offset;
};

inline auto moving_lights_system(Query<Mut<Transform>, MovingLight> query, Res<Time> time)
  -> void {
  f32 t = time->elapsed_seconds;
  for (auto [transform, light] : query.iter()) {
    transform->translation.x =
      (std::cos((t * light->speed) + light->offset) * light->radius);
    transform->translation.z =
      (std::sin((t * light->speed) + light->offset) * light->radius / 2.0f);
  }
}

void setup_scene(
  Cmd cmd, ResMut<Assets<ecs::Mesh>> meshes, ResMut<Assets<ecs::Material>> materials
) {
  auto ground_mesh = meshes->add(Mesh::cube(100.0f, 2.0f, 100.0f));
  auto ground_mat = materials->add(StandardMaterial::from_color(Color::DARKGRAY));

  cmd.spawn(
    Transform::from_xyz(0.0f, -1.0f, 0.0f),
    RigidBody{
      .motion_type = MotionType::Static,
      .collision_layer = Layers::NON_MOVING,
    },
    Collider{
      .shape = Collider::Box{Vec3(50.0f, 1.0f, 50.0f)},
      .friction = 0.8f,
    },
    Mesh3d{ground_mesh},
    MeshMaterial3d{ground_mat}
  );

  cmd.spawn(
    Camera3d::perspective(),
    ActiveCamera{},
    Transform::from_xyz(-50.0f, 42.0f, -50.0f).look_at(Vec3(0.0f, 0.0f, 0.0f)),
    ecs::Environment::from_directory("textures/skybox")
  );

  cmd.spawn(
    DirectionalLight{
      .color = {255, 242, 230, 255},
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.8f,
    }
  );
  for (auto i : range(-10, 10)) {
    for (auto j : range(-10, 10)) {
      if (i % 4 != 0 || j % 4 != 0) {
        continue;
      }

      f32 const angle = (static_cast<f32>(i) / 30.0f) * 2.0f * PI;
      f32 const radius = 30.0f;
      Color color = Color::from_hsv(static_cast<f32>(i * 12), 0.8f, 1.0f);

      cmd.spawn(
        Transform::from_xyz(2.f * i, 12.f, 2.f * j),
        PointLight{
          .color = color,
          .intensity = 40,
          .range = 30,
          .casts_shadows = true,
        },
        MovingLight{
          .radius = radius,
          .speed = 0.2f,
          .offset = angle,
        }
      );
    }
  }
}

void spawn_cubes(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Material>> materials,
  Res<Input> input
) {
  if (!input->key_pressed(KEY_SPACE)) {
    return;
  }
  auto cube_mesh = meshes->add(Mesh::cube());
  auto cube_mat = materials->add(StandardMaterial::from_color(Color{200, 60, 60}));

  for (auto x : range(-10, 10)) {
    for (auto y : range(-10, 10)) {
      cmd.spawn(
        Transform::from_xyz(2.f * x, 20.f, 2.f * y),
        RigidBody{},
        Collider{
          .shape = Collider::Box{Vec3::splat(0.5f)},
          .restitution = 0.3f,
        },
        Mesh3d{cube_mesh},
        MeshMaterial3d{cube_mat}
      );
    }
  }
}

auto main() -> int {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin{
        .window =
          {.width = 1280, .height = 720, .title = "13 - Physics", .target_fps = 120},
        .trace_log_level = LOG_DEBUG,
      }
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin{})
    .add_plugin(InputPlugin{})
    .add_plugin(physics::PhysicsPlugin{})
    .add_systems(Startup, setup_scene)
    .add_systems(Update, spawn_cubes)
    .add_systems(Update, moving_lights_system)
    .run();

  return 0;
}
