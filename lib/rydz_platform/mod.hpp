#pragma once

#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include <cstdio>

namespace rydz_platform {

struct RayPlugin {
  using T = ecs::Resource;

  ecs::Window window{
      .width = 800,
      .height = 600,
      .title = "ECS App",
      .target_fps = 60,
  };
  int trace_log_level = LOG_WARNING;

  static auto install(RayPlugin config) {
    return [config = std::move(config)](ecs::App &app) mutable {
      app.insert_resource(config.window);
      app.insert_resource(config);
      app.insert_resource(ecs::AppRunner{
          .run = [](ecs::App &app_ref) {
            auto *runner = app_ref.world().get_resource<RayPlugin>();
            if (!runner) {
              std::fputs("RayPlugin not installed.\n", stderr);
              return;
            }
            runner->run_app(app_ref);
          },
      });
    };
  }

private:
  ecs::Window resolve_window(ecs::App &app) const {
    if (auto *window = app.world().get_resource<ecs::Window>()) {
      return *window;
    }

    app.insert_resource(this->window);
    return this->window;
  }

  static void sync_window(ecs::App &app) {
    auto *window = app.world().get_resource<ecs::Window>();
    if (!window) {
      return;
    }

    window->width = static_cast<u32>(rl::GetScreenWidth());
    window->height = static_cast<u32>(rl::GetScreenHeight());
  }

  void run_app(ecs::App &app) const {
    ecs::Window config = resolve_window(app);

    rl::SetTraceLogLevel(trace_log_level);
    rl::InitWindow(static_cast<int>(config.width), static_cast<int>(config.height),
                   config.title.c_str());
    rl::SetTargetFPS(static_cast<int>(config.target_fps));
    if (!rl::IsWindowReady()) {
      std::fprintf(stderr, "InitWindow failed; aborting run loop.\n");
      return;
    }

    sync_window(app);
    app.startup();

    while (!rl::WindowShouldClose()) {
      sync_window(app);
      app.update(rl::GetFrameTime());
      FrameMark;
    }

    app.world().resources.clear();
    rl::CloseWindow();
  }
};

using RaylibRunner = RayPlugin;

} // namespace rydz_platform
