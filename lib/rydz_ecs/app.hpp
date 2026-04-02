#pragma once
#include "command.hpp"
#include "core/time.hpp"
#include "event.hpp"
#include "plugin.hpp"
#include "rl.hpp"
#include "schedule.hpp"
#include "state.hpp"
#include "tracy_plugin.hpp"
#include "world.hpp"
#include <string>

namespace ecs {

struct Window {
  using T = ecs::Resource;

  u32 width;
  u32 height;
  std::string title = "ECS App";
  u32 target_fps = 60;

public:
  static void update(ecs::ResMut<Window> window, ecs::NonSendMarker) {
    window->height = rl::GetScreenHeight();
    window->width = rl::GetScreenWidth();
  }

  static auto install(Window config);
};

class App {
  World world_;
  Schedules schedules_;
  bool startup_done_ = false;

public:
  App() {
    world_.insert_resource(CommandQueues{});
    world_.insert_resource(Time{});
  }

  template <typename F> App &add_systems(ScheduleLabel label, F &&func) {
    schedules_.entry(label).add_system_fn(std::forward<F>(func));
    return *this;
  }

  App &configure_set(ScheduleLabel label, SetConfigDescriptor &&desc) {
    schedules_.entry(label).add_set_config(desc.take());
    return *this;
  }

private:
  template <typename S> StateSchedules<S> &get_or_init_state_schedules() {
    auto *schedules = world_.get_resource<StateSchedules<S>>();
    if (!schedules) {
      world_.insert_resource(StateSchedules<S>{});
      schedules = world_.get_resource<StateSchedules<S>>();
    }
    return *schedules;
  }

public:
  template <OnEnterLabel Label, typename F>
  App &add_systems(Label &&label, F &&func) {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_enter[label.value].add_system_fn(
        std::forward<F>(func));
    return *this;
  }

  template <OnExitLabel Label, typename F>
  App &add_systems(Label &&label, F &&func) {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_exit[label.value].add_system_fn(
        std::forward<F>(func));
    return *this;
  }

  template <OnTransitionLabel Label, typename F>
  App &add_systems(Label &&, F &&func) {
    using S = typename bare_t<Label>::state_type;
    get_or_init_state_schedules<S>().on_transition.add_system_fn(
        std::forward<F>(func));
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

  template <typename R, typename... Args> App &init_resource(Args &&...args) {
    if (!world_.has_resource<R>()) {
      world_.insert_resource(R{std::forward<Args>(args)...});
    }
    return *this;
  }

  template <typename S> App &init_state(S initial) {
    world_.insert_resource(State<S>(std::move(initial)));
    world_.insert_resource(NextState<S>{});
    world_.insert_resource(StateTransitionEvent<S>{});
    world_.insert_resource(StateSchedules<S>{});
    schedules_.entry(ScheduleLabel::PreUpdate)
        .add_system(make_system(
            [](World &world_ref) { check_state_transitions<S>(world_ref); }));

    S initial_copy = initial;
    schedules_.entry(ScheduleLabel::Startup)
        .add_system(make_system([initial_copy](World &world_ref) {
          auto *schedules = world_ref.get_resource<StateSchedules<S>>();
          if (schedules) {
            auto it = schedules->on_enter.find(initial_copy);
            if (it != schedules->on_enter.end())
              it->second.run(world_ref);
          }
        }));
    return *this;
  }

  template <IsEvent E> App &add_event() {
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
      time->delta_seconds = rl::GetFrameTime();
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
    schedules_.run(ScheduleLabel::ExtractRender, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Render, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::PostRender, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Last, world_);
    apply_commands();
  }

  void run() {
    auto *config = world_.get_resource<Window>();
    if (config) {
      rl::InitWindow(config->width, config->height, config->title.c_str());
      rl::SetTargetFPS(config->target_fps);
    } else {
      rl::InitWindow(800, 600, "ECS App");
      rl::SetTargetFPS(60);
    }
    if (!rl::IsWindowReady()) {
      rl::TraceLog(LOG_ERROR, "InitWindow failed; aborting run loop.");
      return;
    }

    startup();

    while (!rl::WindowShouldClose()) {
      update();
      FrameMark;
    }

    world_.resources.clear();

    rl::CloseWindow();
  }

private:
  void apply_commands() {
    auto *queues = world_.get_resource<CommandQueues>();
    if (queues && !queues->empty()) {
      world_.increment_change_tick();
      queues->apply(world_);
    }
  }
};

inline void time_plugin(App &app) { app.init_resource<Time>(); }

inline auto Window::install(Window config) {
  return [config = std::move(config)](ecs::App &app) {
    app.add_systems(ScheduleLabel::Update, Window::update);
    app.insert_resource(config);
  };
}
} // namespace ecs
