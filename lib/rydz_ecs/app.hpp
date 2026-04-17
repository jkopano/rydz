#pragma once
#include "command.hpp"
#include "core/time.hpp"
#include "event.hpp"
#include "message.hpp"
#include "plugin.hpp"
#include "schedule.hpp"
#include "state.hpp"
#include "tracy_plugin.hpp"
#include "world.hpp"
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <unordered_map>
#include <vector>

namespace ecs {

class App;

struct Window {
  using T = ecs::Resource;

  u32 width{};
  u32 height{};
  std::string title = "ECS App";
  u32 target_fps{60};

public:
  static auto install(Window config);
};

struct AppRunner {
  using T = Resource;
  std::function<void(App &)> run;
};

class App {
  struct PendingSetSystemsBase {
    PendingSetSystemsBase() = default;
    PendingSetSystemsBase(const PendingSetSystemsBase &) = default;
    PendingSetSystemsBase(PendingSetSystemsBase &&) = delete;
    PendingSetSystemsBase &operator=(const PendingSetSystemsBase &) = default;
    PendingSetSystemsBase &operator=(PendingSetSystemsBase &&) = delete;
    virtual ~PendingSetSystemsBase() = default;
    virtual void attach(App &app) = 0;
  };

  template <typename F> struct PendingSetSystems final : PendingSetSystemsBase {
    SetList sets{};
    decay_t<F> func{};

    PendingSetSystems(SetList selector, F &&system_fn)
        : sets(std::move(selector)), func(std::forward<F>(system_fn)) {}

    void attach(App &app) override {
      auto label = app.resolve_set_schedule(sets);
      app.schedules_.entry(label).add_system_fn(std::move(sets),
                                                std::move(func));
    }
  };

  World world_;
  Schedules schedules_;
  bool startup_done_ = false;
  std::unordered_map<SetId, ScheduleLabel> set_schedules_;
  std::vector<std::unique_ptr<PendingSetSystemsBase>> pending_set_systems_;
#ifndef NDEBUG
  bool debug_graph_dumped_ = false;
#endif

public:
  App() {
    world_.insert_resource(CommandQueues{});
    world_.insert_resource(ObserverRegistry{});
    world_.insert_resource(Time{});
  }

  template <typename F>
    requires SystemInput<F>
  App &add_systems(ScheduleLabel label, F &&func) {
    schedules_.entry(label).add_system_fn(std::forward<F>(func));
    return *this;
  }

  template <typename F>
    requires(!SystemInput<F>)
  App &add_systems(ScheduleLabel /*label*/, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel> && SystemInput<F>)
  App &add_systems(E set_id, F &&func) {
    return add_systems(set(set_id), std::forward<F>(func));
  }

  template <typename E, typename F>
    requires(std::is_enum_v<bare_t<E>> &&
             !std::same_as<bare_t<E>, ScheduleLabel> && !SystemInput<F>)
  App &add_systems(E /*set_id*/, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && SystemInput<F>)
  App &add_systems(S &&, F &&func) {
    return add_systems(set<bare_t<S>>(), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && !SystemInput<F>)
  App &add_systems(S &&, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename S, typename F>
    requires((std::same_as<bare_t<S>, SetId> ||
              std::same_as<bare_t<S>, SetList>) &&
             SystemInput<F>)
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

  template <typename S, typename F>
    requires((std::same_as<bare_t<S>, SetId> ||
              std::same_as<bare_t<S>, SetList>) &&
             !SystemInput<F>)
  App &add_systems(S && /*selector*/, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  App &configure_set(ScheduleLabel label, SetConfigDescriptor &&desc) {
    register_set_schedule(label, desc.id());
    schedules_.entry(label).add_set_config(std::move(desc).take());
    return *this;
  }

  App &configure_set(ScheduleLabel label, ChainedSetConfigDescriptor &&desc) {
    auto configs = std::move(desc).take();
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
    requires SystemInput<F>
  App &add_systems(Label &&label, F &&func) {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_enter[label.value].add_system_fn(
        std::forward<F>(func));
    return *this;
  }

  template <OnEnterLabel Label, typename F>
    requires(!SystemInput<F>)
  App &add_systems(Label && /*label*/, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <OnExitLabel Label, typename F>
    requires SystemInput<F>
  App &add_systems(Label &&label, F &&func) {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_exit[label.value].add_system_fn(
        std::forward<F>(func));
    return *this;
  }

  template <OnExitLabel Label, typename F>
    requires(!SystemInput<F>)
  App &add_systems(Label && /*label*/, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <OnTransitionLabel Label, typename F>
    requires SystemInput<F>
  App &add_systems(Label &&, F &&func) {
    using S = typename bare_t<Label>::state_type;
    get_or_init_state_schedules<S>().on_transition.add_system_fn(
        std::forward<F>(func));
    return *this;
  }

  template <OnTransitionLabel Label, typename F>
    requires(!SystemInput<F>)
  App &add_systems(Label &&, F && /*func*/) {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename F> App &add_plugin(F &&plugin_fn) {
    std::forward<F>(plugin_fn)(*this);
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
            auto iter = schedules->on_enter.find(initial_copy);
            if (iter != schedules->on_enter.end()) {
              iter->second.run(world_ref);
            }
          }
        }));
    return *this;
  }

  template <IsMessage E> App &add_message() {
    if (!world_.has_resource<Messages<E>>()) {
      world_.insert_resource(Messages<E>{});
      schedules_.entry(ScheduleLabel::First)
          .add_system_fn(message_update_system<E>);
    }
    return *this;
  }

  template <IsEvent E> App &add_event() {
    world_.add_event<E>();
    return *this;
  }

  template <typename F> App &add_observer(F &&func) {
    world_.add_observer(std::forward<F>(func));
    return *this;
  }

  World &world() { return world_; }
  const World &world() const { return world_; }
  Schedules &schedules() { return schedules_; }

  void startup() {
    flush_pending_set_systems();
    debug_dump_schedule_graph_once();
    if (startup_done_) {
      return;
    }
    startup_done_ = true;

    schedules_.run(ScheduleLabel::PreStartup, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::Startup, world_);
    apply_commands();
    schedules_.run(ScheduleLabel::PostStartup, world_);
    apply_commands();
  }

  void update(float delta_seconds = 0.0F) {
    flush_pending_set_systems();
    debug_dump_schedule_graph_once();
    auto *time = world_.get_resource<Time>();
    if (time != nullptr) {
      time->delta_seconds = delta_seconds;
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
    auto *runner = world_.get_resource<AppRunner>();
    if ((runner == nullptr) || !runner->run) {
      std::println(stderr,
                   "App::run() called without an AppRunner resource.\n");
      return;
    }
    runner->run(*this);
  }

private:
  void register_set_schedule(ScheduleLabel label, const SetId &set_id) {
    auto [iter, inserted] = set_schedules_.emplace(set_id, label);
    if (!inserted && iter->second != label) {
      throw std::runtime_error(
          "Set '" + set_name(set_id) + "' is configured for both schedules '" +
          std::string(schedule_label_name(iter->second)) + "' and '" +
          std::string(schedule_label_name(label)) + "'");
    }
  }

  std::optional<ScheduleLabel>
  try_resolve_set_schedule(const SetList &sets) const {
    std::optional<ScheduleLabel> label;

    for (const auto &set_id : sets.ids) {
      auto iter = set_schedules_.find(set_id);
      if (iter == set_schedules_.end()) {
        return std::nullopt;
      }

      if (label && *label != iter->second) {
        throw std::runtime_error("Sets '" + detail::set_names(sets.ids) +
                                 "' are configured for different schedules");
      }
      label = iter->second;
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

  void debug_dump_schedule_graph_once() {
#ifndef NDEBUG
    if (debug_graph_dumped_) {
      return;
    }

    schedules_.prepare_all();
    auto dump = schedules_.debug_dump();
    std::fputs(dump.c_str(), stderr);
    std::fputc('\n', stderr);
    debug_graph_dumped_ = true;
#endif
  }

  void apply_commands() {
    auto *queues = world_.get_resource<CommandQueues>();
    if ((queues != nullptr) && !queues->empty()) {
      world_.increment_change_tick();
      queues->apply(world_);
    }
  }
};

inline void time_plugin(App &app) { app.init_resource<Time>(); }

inline auto Window::install(Window config) {
  return [config = std::move(config)](ecs::App &app) {
    app.insert_resource(config);
  };
}

inline auto window_plugin(Window config) {
  return Window::install(std::move(config));
}
} // namespace ecs
