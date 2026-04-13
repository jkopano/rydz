// 10 - Custom Material

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_platform/mod.hpp"

using namespace ecs;
using namespace math;

struct ToonMaterial : MaterialTrait<HasCamera> {
  ecs::Color base_color = kWhite;
  float rim_strength = 0.15f;

  static ShaderRef vertex_shader() { return "res/shaders/toon.vert"; }
  static ShaderRef fragment_shader() { return "res/shaders/toon.frag"; }

  RenderMethod render_method() const {
    return base_color.a < 255 ? RenderMethod::Transparent
                              : RenderMethod::Opaque;
  }

  bool enable_shadows() const { return base_color.a == 255; }

  void bind(MaterialBuilder &builder) const {
    builder.color(MATERIAL_MAP_DIFFUSE, base_color);
    builder.uniform("u_rim_strength", rim_strength);
  }
};

void setup(Cmd cmd, ResMut<Assets<ecs::Mesh>> meshes,
           ResMut<Assets<ecs::Material>> materials, NonSendMarker) {
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 3, 6).look_at(Vec3::sZero()));

  auto sphere = meshes->add(mesh::sphere(1.0f));
  auto floor = meshes->add(mesh::plane(8.0f, 8.0f));
  auto toon = materials->add(ToonMaterial{.base_color = kOrange});
  auto floor_mat =
      materials->add(StandardMaterial::from_color({220, 220, 220, 255}));

  cmd.spawn(Mesh3d{sphere}, MeshMaterial3d{toon},
            Transform::from_xyz(0, 1, 0));
  cmd.spawn(Mesh3d{floor}, MeshMaterial3d{floor_mat}, Transform{});
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
          .window = {800, 600, "10 - Custom Material", 60},
      }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_systems(Startup, setup)
      .run();
}
