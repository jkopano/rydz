// 08 - Hierarchy
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include "rydz_platform/rydz_platform.hpp"
#include <print>

using namespace ecs;
using namespace math;

struct PivotTag {};
struct ArmTag {};

void setup(Cmd cmd, ResMut<Assets<ecs::Mesh>> meshes,
           ResMut<Assets<ecs::Material>> materials, NonSendMarker) {
  // kamera
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 8, 15).look_at(Vec3::sZero()));

  // podłoga
  auto floor_h = meshes->add(mesh::plane(20, 20));
  auto floor_mat = materials->add(StandardMaterial::from_color(kDarkGray));
  cmd.spawn(Mesh3d{floor_h},
            MeshMaterial3d{floor_mat},
            Transform{});

  // światło
  cmd.spawn(DirectionalLight{
      .color = kWhite,
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.6f,
  });

  // parent
  auto cube_h = meshes->add(mesh::cube(1, 1, 1));
  auto yellow_mat = materials->add(StandardMaterial::from_color(kYellow));
  auto pivot =
      cmd.spawn(Mesh3d{cube_h}, MeshMaterial3d{yellow_mat},
                Transform::from_xyz(0, 2, 0), PivotTag{});

  // child
  auto arm_h = meshes->add(mesh::cube(3, 0.4f, 0.4f));
  auto red_mat = materials->add(StandardMaterial::from_color(kRed));
  auto arm =
      cmd.spawn(Mesh3d{arm_h}, MeshMaterial3d{red_mat},
                Transform::from_xyz(2.0f, 0, 0), // offset od pivota
                Parent{pivot.id()}, ArmTag{});

  // child of child
  auto tip_h = meshes->add(mesh::sphere(0.4f));
  auto blue_mat = materials->add(StandardMaterial::from_color(kBlue));
  cmd.spawn(Mesh3d{tip_h},
            MeshMaterial3d{blue_mat},
            Transform::from_xyz(1.8f, 0, 0), // offset od ramienia
            Parent{arm.id()});

  std::println("hierarchy: pivot({}) -> arm({}) -> tip", pivot.id().index(),
               arm.id().index());
}

// filtrujesz entity które mają Parent
void system(Query<Transform, Parent> children_query) {
  for (auto [t, parent] : children_query.iter()) {
    // parent->entity
  }
}

// znajdź entity bez rodzica
void system(Query<Transform, Opt<Parent>> query) {
  for (auto [t, parent] : query.iter()) {
    if (!parent) {
      // to jest root
    }
  }
}

// pobierz dzieci danego entity
// raczej nie korzystać, się lepiej to potem napisze
// Children kids = children_of(world, some_entity);
// for (Entity child : kids) {
//   // ...
// }

// Obracamy rodzica - dzieci powinny się obracać razem z nim
void rotate_pivot(Query<Mut<Transform>, PivotTag> query, Res<Time> time) {
  for (auto [t, _] : query.iter()) {
    f32 angle = time->delta_seconds * 1.0f;
    t->rotation = Quat::sEulerAngles(Vec3(0, angle, 0)) * t->rotation;
  }
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
          .window = {1024, 768, "08 - Hierarchy", 60},
      }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, rotate_pivot)
      .run();
}
