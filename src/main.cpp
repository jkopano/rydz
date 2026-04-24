#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT 0
#include "rydz_platform/mod.hpp"
// #include "scene_new.hpp"
#include "scene.hpp"

auto main() -> int {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin{
        .window =
          {.width = 1280, .height = 720, .title = "rydz_ecs 3D Demo", .target_fps = 800},
        .trace_log_level = LOG_DEBUG,
      }
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin{})
    .add_plugin(Input::install)
    .add_plugin(scene_plugin)
    .run();

  return 0;
}
