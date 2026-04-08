#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT 0
#include "scene_new.hpp"
#include "rydz_platform/rydz_platform.hpp"

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
          .window = {1280, 720, "rydz_ecs 3D Demo", 800},
          .trace_log_level = LOG_DEBUG,
      }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_plugin(scene_plugin)
      .run();

  return 0;
}
