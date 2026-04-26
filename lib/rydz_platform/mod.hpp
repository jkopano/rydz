#pragma once

#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include <cstdio>
#include <print>

#include "rydz_log/mod.hpp"

namespace rydz_platform {

struct RayPlugin {
  using T = ecs::Resource;

  ecs::Window window{
    .width = 800,
    .height = 600,
    .title = "ECS App",
    .target_fps = 60,
  };
  i32 trace_log_level = LOG_NONE;

  static auto install(RayPlugin config) {
    return [config = std::move(config)](ecs::App& app) mutable -> void {
      app.insert_resource(config.window);
      app.insert_resource(config);
      app.insert_resource(
        ecs::AppRunner{
          .run = [](ecs::App& app_ref) -> void {
            auto* runner = app_ref.world().get_resource<RayPlugin>();
            if (!runner) {
              std::fputs("RayPlugin not installed.\n", stderr);
              return;
            }
            runner->run_app(app_ref);
          },
        }
      );
    };
  }

private:
  auto resolve_window(ecs::App& app) const -> ecs::Window {
    if (auto* window = app.world().get_resource<ecs::Window>()) {
      return *window;
    }

    app.insert_resource(this->window);
    return this->window;
  }

  static auto sync_window(ecs::App& app) -> void {
    auto* window = app.world().get_resource<ecs::Window>();
    if (window == nullptr) {
      return;
    }

    window->width = static_cast<u32>(rl::GetScreenWidth());
    window->height = static_cast<u32>(rl::GetScreenHeight());
  }

  auto run_app(ecs::App& app) const -> void {
    ecs::Window config = resolve_window(app);

    if (config.is_fullscreen) {
      rl::SetConfigFlags(FLAG_FULLSCREEN_MODE);
    }

    app.add_plugin(LogPlugin{});

    rl::InitWindow(
      static_cast<int>(config.width),
      static_cast<int>(config.height),
      config.title.c_str()
    );
    rl::SetTargetFPS(static_cast<int>(config.target_fps));
    if (!rl::IsWindowReady()) {
      std::println(stderr, "InitWindow failed; aborting run loop.");
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
