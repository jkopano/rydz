#define RLGL_ENABLE_OPENGL_DEBUG_CONTEXT 0
// #include "rl.hpp"
#include "scene.hpp"
// #include "scene_new.hpp"

int main() {
  rl::SetTraceLogLevel(LOG_DEBUG);
  App app;
  app.add_plugin(Window::install({1280, 720, "rydz_ecs 3D Demo", 800}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_plugin(scene_plugin)
      .run();

  return 0;
}
