// 10 - Custom Material

#include "rydz_graphics/render_plugin.hpp"
#include "rydz_platform/mod.hpp"
#include <cmath>
#include <string_view>

using namespace ecs;
using namespace math;

enum class ToonUniform {
  RimStrength,
  LightDir,
};

static inline auto map_uniform_binding(ToonUniform uniform)
  -> std::string_view {
  switch (uniform) {
  case ToonUniform::RimStrength:
    return "u_rim_strength";
  case ToonUniform::LightDir:
    return "lightDir";
  }
  return "";
}

struct ToonMaterial : MaterialTrait<HasCamera> {
  Color base_color = Color::WHITE;
  float rim_strength = 0.15f;
  Vec3 lightDir = {1.0, -1.0, 0.1};

  static auto vertex_shader() -> ShaderRef { return "res/shaders/toon.vert"; }
  static auto fragment_shader() -> ShaderRef { return "res/shaders/toon.frag"; }

  [[nodiscard]] auto render_method() const -> RenderMethod {
    return base_color.a < 255 ? RenderMethod::Transparent
                              : RenderMethod::Opaque;
  }

  [[nodiscard]] auto enable_shadows() const -> bool {
    return base_color.a == 255;
  }

  void bind(MaterialBuilder& builder) const {
    builder.color(MaterialMap::Albedo, base_color);
    builder.uniform(ToonUniform::RimStrength, Uniform{rim_strength});
    builder.uniform("lightDir", Uniform{lightDir});
  }
};

namespace {
auto change_toon(
  Query<Entity, MeshMaterial3d<ToonMaterial>> query,
  ResMut<Assets<ToonMaterial>> materials,
  Res<Time> time
) -> void {
  for (auto [entity, material_handle] : query.iter()) {
    if (auto* mat = materials->get(material_handle->material)) {
      mat->lightDir.x = std::cos(time->elapsed_seconds);
      mat->lightDir.z = std::sin(time->elapsed_seconds);
    }
  }
}

auto setup(
  Cmd cmd,
  ResMut<Assets<ecs::Mesh>> meshes,
  ResMut<Assets<ToonMaterial>> toon_materials,
  ResMut<Assets<StandardMaterial>> standard_materials
) -> void {
  cmd.spawn(
    Camera3d::perspective(60.0f),
    ActiveCamera{},
    Transform::from_xyz(0, 3, 6).look_at(Vec3::ZERO)
  );

  auto sphere = meshes->add(mesh::sphere(1.0f));
  auto floor = meshes->add(mesh::plane(8.0f, 8.0f));
  auto toon =
    toon_materials->add(ToonMaterial{.base_color = ecs::Color::ORANGE});
  auto floor_mat =
    standard_materials->add(StandardMaterial::from_color({220, 220, 220, 255}));

  cmd.spawn(
    Mesh3d{sphere},
    MeshMaterial3d<ToonMaterial>{toon},
    Transform::from_xyz(0, 1, 0)
  );
  cmd.spawn(
    Mesh3d{floor}, MeshMaterial3d<StandardMaterial>{floor_mat}, Transform{}
  );
}
} // namespace

auto main() -> int {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin::install({
        .window =
          {.width = 800,
           .height = 600,
           .title = "10 - Custom Material",
           .target_fps = 60},
      })
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin::install)
    .add_plugin(RenderPlugin::register_material<ToonMaterial>)
    .add_systems(Startup, setup)
    .add_systems(Update, change_toon)
    .run();
}
