// 10 - Custom Material

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"

using namespace ecs;
using namespace math;

//////////////////////////////////////////////////////////////////////////////////
//                                                                               /
//                                                                               /
// GENERALNIE TO NIE DZIAŁA NA RAZIE XD ALE NAWET JAKBY DZIAŁAŁO TO NAPEWNO DO
// POPRAWY 100%, ale no pipeline pi razy drzwi tak by właśnie wyglądał
//                                                                               /
//                                                                               /
//////////////////////////////////////////////////////////////////////////////////

// Własny materiał musi spełnić concept IMaterial:
//   color(), apply(), vertex_source(), fragment_source(), shader()
struct ToonMaterial {
  rl::Color base_color = WHITE;

  rl::Color color() const { return base_color; }

  void apply(rl::Model &model, Assets<rl::Texture2D> *) const {
    const rl::Shader *s = shader();
    if (s) {
      for (int i = 0; i < model.materialCount; ++i)
        model.materials[i].shader = *s;
    }
    model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = base_color;
  }

  static const char *vertex_source() { return "res/shaders/toon.vert"; }
  static const char *fragment_source() { return "res/shaders/toon.frag"; }

  static const rl::Shader *shader() {
    static rl::Shader s = [] {
      rl::Shader sh = rl::LoadShader(ToonMaterial::vertex_source(),
                                     ToonMaterial::fragment_source());
      if (sh.id == 0) {
        sh.id = rl::rlGetShaderIdDefault();
        sh.locs = rl::rlGetShaderLocsDefault();
      } else {
        sh.locs[SHADER_LOC_VERTEX_POSITION] =
            rl::GetShaderLocationAttrib(sh, "vertexPosition");
        sh.locs[SHADER_LOC_VERTEX_NORMAL] =
            rl::GetShaderLocationAttrib(sh, "vertexNormal");
        sh.locs[SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(sh, "mvp");
        sh.locs[SHADER_LOC_MATRIX_MODEL] =
            rl::GetShaderLocation(sh, "matModel");
        sh.locs[SHADER_LOC_COLOR_DIFFUSE] =
            rl::GetShaderLocation(sh, "colDiffuse");
      }
      return sh;
    }();
    return &s;
  }
};

using ToonMat = MeshMaterial3d<ToonMaterial>;

void setup(Cmd cmd, ResMut<Assets<rl::Model>> models, NonSendMarker) {
  cmd.spawn(Camera3DComponent{60.0}, ActiveCamera{},
            Transform::from_xyz(0, 3, 6).look_at(Vec3::sZero()));

  auto h = models->add(rl::LoadModelFromMesh(mesh::sphere(1.0f)));
  cmd.spawn(Model3d{h}, ToonMat{ToonMaterial{.base_color = ORANGE}},
            Transform::from_xyz(0, 1, 0));
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "10 - Custom Material", 60}))
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_systems(ScheduleLabel::Startup, setup)
      .run();
}
