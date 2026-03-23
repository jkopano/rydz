// 06 - Rendering
// Pokazuje: mesh::cube/sphere/cylinder, Model3d, Material3d, Transform3D,
//           Skybox, Assets, AssetServer (GLTF), Textures
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"

using namespace ecs;
using namespace math;

struct RotateTag {};

// NonSendMarker musi być gdy funkcja musi być odpalona na głównym wątku
// (inną opcją jest dodanie do funkcji World world)
void setup(Cmd cmd, ResMut<Assets<rl::Model>> models, NonSendMarker) {
  // kamera ze skyboxem — Skybox ładuje 6 tekstur z folderu (kinda słabe do
  // poprawy) (right/left/top/bottom/front/back.jpg) Skybox na razie musi być w
  // kamerze, chyba dobre rozwiązanie, ale do ugadania
  cmd.spawn(Camera3DComponent{60.0}, ActiveCamera{},
            ecs::Transform::from_xyz(0, 5, 12).look_at(Vec3::sZero()),
            Skybox::from("res/hdri/skybox"));

  // cube - czerwona
  auto cube = rl::LoadModelFromMesh(mesh::cube(2, 2, 2));
  auto cube_h = models->add(std::move(cube));
  cmd.spawn(Model3d{cube_h}, Material3d{StandardMaterial::from_color(RED)},
            ecs::Transform::from_xyz(-4, 1, 0), RotateTag{});

  // kula - zielona
  auto sphere = rl::LoadModelFromMesh(mesh::sphere(1.0f));
  auto sphere_h = models->add(std::move(sphere));
  cmd.spawn(Model3d{sphere_h}, Material3d{StandardMaterial::from_color(GREEN)},
            ecs::Transform::from_xyz(0, 1, 0));

  // cylinder - niebieski
  auto cyl = rl::LoadModelFromMesh(mesh::cylinder(0.8f, 2.0f));
  auto cyl_h = models->add(std::move(cyl));
  cmd.spawn(Model3d{cyl_h}, Material3d{StandardMaterial::from_color(BLUE)},
            ecs::Transform::from_xyz(4, 1, 0));

  // floor
  auto floor = rl::LoadModelFromMesh(mesh::plane(20, 20));
  auto floor_h = models->add(std::move(floor));
  cmd.spawn(Model3d{floor_h},
            Material3d{StandardMaterial::from_color(DARKGRAY)},
            ecs::Transform::from_xyz(0, 0, 0));

  // torus
  auto torus = rl::LoadModelFromMesh(mesh::torus(1.0f, 0.3f));
  auto torus_h = models->add(std::move(torus));
  cmd.spawn(Model3d{torus_h}, Material3d{StandardMaterial::from_color(PURPLE)},
            ecs::Transform::from_xyz(0, 3, -4), RotateTag{});

  // światło żeby coś było widać
  cmd.spawn(DirectionalLight{
      .color = WHITE,
      .direction = Vec3(-0.3f, -1.0f, -0.5f),
      .intensity = 0.5f,
  });
}

// Asset Server - ładowanie modeli GLTF z plików
void load_gltf_model(Cmd cmd, Res<AssetServer> asset_server) {
  // load() zwraca Handle<Model> od razu — model ładuje się w tle
  // jak plik nie istnieje to nic się nie renderuje ale bez crasha
  auto model_handle = asset_server->load<rl::Model>("res/models/old_house.glb");

  cmd.spawn(Model3d{model_handle},
            ecs::Transform{.translation = Vec3(8, 0, 0),
                           .scale = Vec3::sReplicate(0.02f)});
}

// ładowanie tekstur i nakładanie na materiał
void load_textured_cube(Cmd cmd, ResMut<Assets<rl::Model>> models,
                        ResMut<Assets<rl::Texture2D>> textures, NonSendMarker) {
  // ładowanko tekstury można też przez AssetServer
  auto tex_handle = textures->add(rl::LoadTexture("res/textures/stone.jpg"));

  // materiał z teksturą
  auto mat = StandardMaterial::from_texture(tex_handle);

  auto cube = rl::LoadModelFromMesh(mesh::cube(2, 2, 2));
  auto cube_h = models->add(std::move(cube));

  cmd.spawn(Model3d{cube_h}, Material3d{mat},
            ecs::Transform::from_xyz(-8, 1, 0), RotateTag{});
}

// Obracamy obiekty z RotateTag
void rotate_system(Query<Mut<Transform>, With<RotateTag>> query,
                   Res<Time> time) {
  for (auto [t] : query.iter()) {
    f32 angle = time->delta_seconds * 1.0f;
    Quat rot = Quat::sRotation(Vec3(0, 1, 0), angle);
    t->rotation = rot * t->rotation;
  }
}

int main() {
  App app;
  app.add_plugin(window_plugin({1024, 768, "06 - Rendering", 60}))
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Startup, load_textured_cube)
      .add_systems(ScheduleLabel::Update,
                   group(load_gltf_model).run_if(run_once()))
      .add_systems(ScheduleLabel::Update, rotate_system)
      .run();
}
