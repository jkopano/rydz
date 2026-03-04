#pragma once
#include "command.hpp"
#include "event.hpp"
#include "plugin.hpp"
#include "schedule.hpp"
#include "state.hpp"
#include "time.hpp"
#include "world.hpp"
#include <raylib.h>
#include <raymath.h>
#include <string>

namespace ecs {

struct WindowConfig {
  int width = 800;
  int height = 600;
  std::string title = "ECS App";
  int target_fps = 60;
};

class App {
  World world_;
  Schedules schedules_;
  bool startup_done_ = false;

public:
  App() {
    world_.insert_resource(CommandQueue{});
    world_.insert_resource(Time{});
  }

  template <typename F> App &add_systems(ScheduleLabel label, F &&func) {
    if constexpr (is_system_descriptor_v<std::decay_t<F>>) {
      schedules_.entry(label).add_system(func.build());
    } else {
      schedules_.entry(label).add_system_fn(std::forward<F>(func));
    }
    return *this;
  }

  template <typename F, typename Cond>
  App &add_systems_if(ScheduleLabel label, F &&func, Cond &&cond) {
    schedules_.entry(label).add_system_fn_if(std::forward<F>(func),
                                             std::forward<Cond>(cond));
    return *this;
  }

  template <typename F> App &add_plugin(F &&plugin_fn) {
    plugin_fn(*this);
    return *this;
  }

  App &add_plugin(IPlugin &plugin) {
    plugin.build(*this);
    return *this;
  }

  template <typename R> App &insert_resource(R resource) {
    world_.insert_resource(std::move(resource));
    return *this;
  }

  template <typename S> App &init_state(S initial) {
    world_.insert_resource(State<S>(std::move(initial)));
    world_.insert_resource(NextState<S>{});
    schedules_.entry(ScheduleLabel::PreUpdate)
        .add_system(make_system(
            [](World &world_ref) { check_state_transitions<S>(world_ref); }));
    return *this;
  }

  template <typename E> App &add_event() {
    world_.insert_resource(Events<E>{});
    schedules_.entry(ScheduleLabel::First)
        .add_system_fn(event_update_system<E>);
    return *this;
  }

  World &world() { return world_; }
  const World &world() const { return world_; }
  Schedules &schedules() { return schedules_; }

  void startup() {
    if (startup_done_)
      return;
    startup_done_ = true;

    schedules_.run(ScheduleLabel::PreStartup, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Startup, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::PostStartup, world_);
    apply_commands();
  }

  void update() {
    auto *time = world_.get_resource<Time>();
    if (time) {
      time->delta_seconds = GetFrameTime();
      time->elapsed_seconds += time->delta_seconds;
      time->frame_count++;
    }

    world_.increment_change_tick();

    schedules_.run(ScheduleLabel::First, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::PreUpdate, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Update, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::PostUpdate, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Last, world_);
    apply_commands();
  }

  void run() {
    auto *config = world_.get_resource<WindowConfig>();
    if (config) {
      InitWindow(config->width, config->height, config->title.c_str());
      SetTargetFPS(config->target_fps);
    } else {
      InitWindow(800, 600, "ECS App");
      SetTargetFPS(60);
    }

    startup();

    while (!WindowShouldClose()) {
      update();
    }

    CloseWindow();
  }

private:
  void apply_commands() {
    auto *queue = world_.get_resource<CommandQueue>();
    if (queue && !queue->empty()) {
      queue->apply(world_);
    }
  }
};

inline auto window_plugin(WindowConfig config = {}) {
  return
      [config = std::move(config)](App &app) { app.insert_resource(config); };
}

inline void time_plugin(App &app) {
  if (!app.world().contains_resource<Time>()) {
    app.insert_resource(Time{});
  }
}

} // namespace ecs
