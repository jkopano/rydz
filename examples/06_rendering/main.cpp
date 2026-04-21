// 06 - Rendering
// Pokazuje: mesh::cube/sphere/cylinder, Mesh3d,
//           MeshMaterial3d<>, Transform3D,
//           Skybox, Assets, AssetServer (GLTF), Textures
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/plugin.hpp"
#include "rydz_platform/mod.hpp"

using namespace ecs;
using namespace math;

struct RotateTag {};

// NonSendMarker musi być gdy funkcja musi być odpalona na głównym wątku
// (inną opcją jest dodanie do funkcji World world)
void setup(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  // kamera ze skyboxem — Skybox ładuje 6 tekstur z folderu (kinda słabe do
  // poprawy) (right/left/top/bottom/front/back.jpg) Skybox na razie musi być w
  // kamerze, chyba dobre rozwiązanie, ale do ugadania
  cmd.spawn(
    Camera3d::perspective(60.0f),
    ActiveCamera{},
    ecs::Transform::from_xyz(0, 5, 12).look_at(Vec3::ZERO),
    Skybox::from("res/hdri/skybox")
  );

  // cube - czerwona
  auto cube_h = meshes->add(mesh::cube(2, 2, 2));
  auto cube_mat = materials->add(StandardMaterial::from_color(ecs::Color::RED));
  cmd.spawn(
    Mesh3d{cube_h},
    MeshMaterial3d{cube_mat},
    ecs::Transform::from_xyz(-4, 1, 0),
    RotateTag{}
  );

  // kula - zielona
  auto sphere_h = meshes->add(mesh::sphere(1.0f));
  auto sphere_mat =
    materials->add(StandardMaterial::from_color(ecs::Color::GREEN));
  cmd.spawn(
    Mesh3d{sphere_h},
    MeshMaterial3d{sphere_mat},
    ecs::Transform::from_xyz(0, 1, 0)
  );

  // cylinder - niebieski
  auto cyl_h = meshes->add(mesh::cylinder(0.8f, 2.0f));
  auto cyl_mat = materials->add(StandardMaterial::from_color(ecs::Color::BLUE));
  cmd.spawn(
    Mesh3d{cyl_h}, MeshMaterial3d{cyl_mat}, ecs::Transform::from_xyz(4, 1, 0)
  );

  // floor
  auto floor_h = meshes->add(mesh::plane(20, 20));
  auto floor_mat =
    materials->add(StandardMaterial::from_color(ecs::Color::DARKGRAY));
  cmd.spawn(
    Mesh3d{floor_h},
    MeshMaterial3d{floor_mat},
    ecs::Transform::from_xyz(0, 0, 0)
  );

  // torus
  auto torus_h = meshes->add(mesh::torus(1.0f, 0.3f));
  auto torus_mat =
    materials->add(StandardMaterial::from_color(ecs::Color::PURPLE));
  cmd.spawn(
    Mesh3d{torus_h},
    MeshMaterial3d{torus_mat},
    ecs::Transform::from_xyz(0, 3, -4),
    RotateTag{}
  );

  // światło żeby coś było widać
  cmd.spawn(
    DirectionalLight{
      .color = ecs::Color::WHITE,
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.5f,
    }
  );
}

// Asset Server - ładowanie modeli GLTF z plików
void load_gltf_model(Cmd cmd, Res<AssetServer> asset_server) {
  // load() zwraca Handle<Scene> od razu — scena ładuje się w tle
  // jak plik nie istnieje to nic się nie renderuje ale bez crasha
  auto model_handle = asset_server->load<Scene>("res/models/old_house.glb");

  cmd.spawn(
    SceneRoot{model_handle},
    ecs::Transform{.translation = Vec3(8, 0, 0), .scale = Vec3::splat(0.02f)}
  );
}

// ładowanie tekstur i nakładanie na materiał
void load_textured_cube(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ecs::Texture>> textures,
  ResMut<Assets<ecs::Material>> materials,
  NonSendMarker
) {
  // ładowanko tekstury można też przez AssetServer
  auto tex_handle = textures->add(gl::load_texture("res/textures/stone.jpg"));

  // materiał z teksturą
  auto mat = materials->add(StandardMaterial::from_texture(tex_handle));

  auto cube_h = meshes->add(mesh::cube(2, 2, 2));

  cmd.spawn(
    Mesh3d{cube_h},
    MeshMaterial3d{mat},
    ecs::Transform::from_xyz(-8, 1, 0),
    RotateTag{}
  );
}

// Obracamy obiekty z RotateTag
void rotate_system(
  Query<Mut<Transform>, With<RotateTag>> query, Res<Time> time
) {
  for (auto [t] : query.iter()) {
    f32 angle = time->delta_seconds * 1.0f;
    Quat rot = Quat::from_euler(Vec3(0, angle, 0));
    t->rotation = rot * t->rotation;
  }
}

int main() {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin::install({
        .window = {1024, 768, "06 - Rendering", 60},
      })
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin::install)
    .add_systems(Startup, setup)
    .add_systems(Startup, load_textured_cube)
    .add_systems(Update, group(load_gltf_model).run_if(run_once()))
    .add_systems(Update, rotate_system)
    .run();
}
