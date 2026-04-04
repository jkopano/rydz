// 10 - Custom Material

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"

using namespace ecs;
using namespace math;

struct ToonMaterial {
  rl::Color base_color = WHITE;
  float rim_strength = 0.15f;

  MaterialDescriptor describe() const {
    MaterialDescriptor descriptor;
    descriptor.shader = {
        .vertex_path = "res/shaders/toon.vert",
        .fragment_path = "res/shaders/toon.frag",
    };
    descriptor.flags.transparent = base_color.a < 255;
    descriptor.flags.casts_shadows = base_color.a == 255;
    descriptor.maps.push_back(
        MaterialMapBinding::color_binding(MATERIAL_MAP_DIFFUSE, base_color));
    descriptor.uniforms.push_back(
        ShaderUniformValue::float1("u_rim_strength", rim_strength));
    return descriptor;
  }
};

using ToonMat = MeshMaterial3d<ToonMaterial>;

void setup(Cmd cmd, ResMut<Assets<rl::Mesh>> meshes, NonSendMarker) {
  cmd.spawn(Camera3DComponent::perspective(60.0f), ActiveCamera{},
            Transform::from_xyz(0, 3, 6).look_at(Vec3::sZero()));

  auto sphere = meshes->add(mesh::sphere(1.0f));
  auto floor = meshes->add(mesh::plane(8.0f, 8.0f));

  cmd.spawn(Mesh3d{sphere}, ToonMat{ToonMaterial{.base_color = ORANGE}},
            Transform::from_xyz(0, 1, 0));
  cmd.spawn(Mesh3d{floor},
            MeshMaterial3d<>{
                StandardMaterial::from_color({220, 220, 220, 255})},
            Transform{});
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "10 - Custom Material", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install<ToonMaterial>)
      .add_systems(ScheduleLabel::Startup, setup)
      .run();
}
