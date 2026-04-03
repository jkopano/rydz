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
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
  struct PendingSetSystemsBase {
    virtual ~PendingSetSystemsBase() = default;
    virtual void attach(App &app) = 0;
  };

  template <typename F> struct PendingSetSystems final : PendingSetSystemsBase {
    SetList sets;
    bare_t<F> func;

    PendingSetSystems(SetList selector, F &&system_fn)
        : sets(std::move(selector)), func(std::forward<F>(system_fn)) {}

    void attach(App &app) override {
      auto label = app.resolve_set_schedule(sets);
      app.schedules_.entry(label).add_system_fn(std::move(sets), std::move(func));
    }
  };

  World world_;
  Schedules schedules_;
  bool startup_done_ = false;
  std::unordered_map<SetId, ScheduleLabel> set_schedules_;
  std::vector<std::unique_ptr<PendingSetSystemsBase>> pending_set_systems_;

public:
  App() {
    world_.insert_resource(CommandQueues{});
    world_.insert_resource(Time{});
  }

  template <typename F> App &add_systems(ScheduleLabel label, F &&func) {
    schedules_.entry(label).add_system_fn(std::forward<F>(func));
    return *this;
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel>)
  App &add_systems(E set_id, F &&func) {
    return add_systems(set(set_id), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S>)
  App &add_systems(S &&, F &&func) {
    return add_systems(set<bare_t<S>>(), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(std::same_as<bare_t<S>, SetId> || std::same_as<bare_t<S>, SetList>)
  App &add_systems(S &&selector, F &&func) {
    auto sets = SetList{detail::to_set_ids(std::forward<S>(selector))};
    detail::ensure_unique_sets(sets.ids, "add_systems(...)");

    if (auto label = try_resolve_set_schedule(sets)) {
      schedules_.entry(*label).add_system_fn(std::move(sets),
                                             std::forward<F>(func));
      return *this;
    }

    pending_set_systems_.push_back(std::make_unique<PendingSetSystems<F>>(
        std::move(sets), std::forward<F>(func)));
    return *this;
  }

  App &configure_set(ScheduleLabel label, SetConfigDescriptor &&desc) {
    register_set_schedule(label, desc.id());
    schedules_.entry(label).add_set_config(desc.take());
    return *this;
  }

  App &configure_set(ScheduleLabel label, ChainedSetConfigDescriptor &&desc) {
    auto configs = desc.take();
    for (const auto &config : configs) {
      register_set_schedule(label, config.id);
    }
    for (auto &config : configs) {
      schedules_.entry(label).add_set_config(std::move(config));
    }
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
    flush_pending_set_systems();
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
    flush_pending_set_systems();
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
  void register_set_schedule(ScheduleLabel label, const SetId &set_id) {
    auto [it, inserted] = set_schedules_.emplace(set_id, label);
    if (!inserted && it->second != label) {
      throw std::runtime_error("Set '" + set_name(set_id) +
                               "' is configured for both schedules '" +
                               std::string(schedule_label_name(it->second)) +
                               "' and '" +
                               std::string(schedule_label_name(label)) + "'");
    }
  }

  std::optional<ScheduleLabel> try_resolve_set_schedule(const SetList &sets) const {
    std::optional<ScheduleLabel> label;

    for (const auto &set_id : sets.ids) {
      auto it = set_schedules_.find(set_id);
      if (it == set_schedules_.end()) {
        return std::nullopt;
      }

      if (label && *label != it->second) {
        throw std::runtime_error("Sets '" + detail::set_names(sets.ids) +
                                 "' are configured for different schedules");
      }
      label = it->second;
    }

    return label;
  }

  ScheduleLabel resolve_set_schedule(const SetList &sets) const {
    auto label = try_resolve_set_schedule(sets);
    if (!label) {
      throw std::runtime_error("Sets '" + detail::set_names(sets.ids) +
                               "' were used by add_systems(...) but were not "
                               "configured");
    }
    return *label;
  }

  void flush_pending_set_systems() {
    if (pending_set_systems_.empty()) {
      return;
    }

    auto pending = std::move(pending_set_systems_);
    pending_set_systems_.clear();
    for (auto &entry : pending) {
      entry->attach(*this);
    }
  }

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

inline auto window_plugin(Window config) {
  return Window::install(std::move(config));
}
} // namespace ecs
