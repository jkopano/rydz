#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT 0
#include "rydz_audio/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_platform/mod.hpp"
//#include "scene_new.hpp"
#include "scene_lua.hpp"
//#include "scene.hpp"

using namespace ecs;

auto main() -> int {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin{
        .window = {.fullscreen = true, .title = "rydz_ecs 3D Demo", .target_fps = 800},
        .trace_log_level = LOG_DEBUG,
      }
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin{})
    .add_plugin(InputPlugin{})
    .add_plugin(rydz_audio::AudioPlugin{})
    .add_plugin(scene_lua_plugin)
    .run();

  return 0;
}
