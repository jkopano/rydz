// 01 - Hello Window
// Pokazuje: App, Window, pluginy, ClearColor, prosty system Update

#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_platform/mod.hpp"
#include <print>

using namespace ecs;

void setup(Cmd cmd) {
  cmd.spawn(Camera3DComponent::perspective(), ActiveCamera{},
            ClearColor{{40, 80, 120, 255}}, Transform{});
}

void hello_system(Res<Time> time) {
  if (static_cast<int>(time->elapsed_seconds) % 2 == 0 &&
      time->frame_count % 120 == 1) {
    std::println("elapsed: {:.1f}s  frame: {}", time->elapsed_seconds,
                 time->frame_count);
  }
}

int main() {
  // Tworzymy App i dodajemy pluginy - pluginy to tylko jakiś sposób/konwencja
  // na grupowanie systemów/logiki etc
  App app;
  // defaultowe pluginy z ecs na razie to:
  // - time_plugin
  // - window_plugin (jest o tyle różny że to funkcja zwracająca funkcje)
  // - input_plugin
  // - render_plugin
  app.add_plugin(rydz_platform::RayPlugin::install({
          .window = {800, 600, "01 - Hello Window", 60},
      }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_systems(Startup, setup)
      .add_systems(Update, hello_system)
      .run();
}
