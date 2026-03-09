#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT 0
#include "raylib.h"
#include "scene.hpp"
int main() {
  SetTraceLogLevel(LOG_DEBUG);
  App app;
  app.add_plugin(window_plugin({1920, 1200, "rydz_ecs 3D Demo", 120}))
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_plugin(scene_plugin)
      .run();

  return 0;
}
