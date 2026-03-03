#include "rydz_ecs/rydz_ecs.hpp"

using namespace ecs;

void setup(Cmd cmd, ResMut<Assets<Model>> meshes) {
  auto cube_h = meshes->add(mesh::cube(1, 1, 1));
  auto sphere_h = meshes->add(mesh::sphere(0.5f));
  auto plane_h = meshes->add(mesh::plane(20, 20));

  cmd.spawn(Mesh3d{plane_h}, Material3d{Color{60, 60, 70, 255}},
            Transform3D::from_xyz(0, 0, 0), GlobalTransform{},
            Visibility::Visible);

  Color colors[] = {RED, GREEN, BLUE, YELLOW, PURPLE, ORANGE};
  for (int i = 0; i < 6; ++i) {
    float angle = (float)i / 6.0f * 2.0f * PI;
    float x = cosf(angle) * 4.0f;
    float z = sinf(angle) * 4.0f;

    cmd.spawn(Mesh3d{cube_h}, Material3d{colors[i]},
              Transform3D::from_xyz(x, 0.5f, z), GlobalTransform{},
              Visibility::Visible);
  }

  cmd.spawn(Mesh3d{sphere_h}, Material3d{SKYBLUE},
            Transform3D::from_xyz(0, 1.0f, 0), GlobalTransform{},
            Visibility::Visible);

  cmd.spawn(Camera3DComponent{45.0f}, ActiveCamera{},
            Transform3D::from_xyz(8, 6, 8).look_at({0, 0, 0}),
            GlobalTransform{});
}

void rotate_cubes(Query<Write<Transform3D>, Read<Mesh3d>> query,
                  Res<Time> time) {
  query.for_each([&](Transform3D *t, const Mesh3d *) {
    if (t->translation.y > 0.1f) {
      Quaternion rot = QuaternionFromEuler(0, time->delta_seconds * 0.5f, 0);
      t->rotation = QuaternionMultiply(t->rotation, rot);
    }
  });
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "rydz_ecs 3D Demo", 60}))
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, rotate_cubes)
      .run();

  return 0;
}
