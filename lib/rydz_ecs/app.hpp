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
#include <utility>
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
  std::function<void(App&)> run;
};

namespace detail {

template <typename P>
concept BuildPlugin = requires(P&& plugin, App& app) {
  std::forward<P>(plugin).build(app);
};

template <typename F>
concept CallablePlugin = requires(F&& plugin_fn, App& app) {
  std::forward<F>(plugin_fn)(app);
};

} // namespace detail

class App {
  struct PendingSetSystemsBase {
    PendingSetSystemsBase() = default;
    PendingSetSystemsBase(PendingSetSystemsBase const&) = default;
    PendingSetSystemsBase(PendingSetSystemsBase&&) = delete;
    auto operator=(PendingSetSystemsBase const&)
      -> PendingSetSystemsBase& = default;
    auto operator=(PendingSetSystemsBase&&) -> PendingSetSystemsBase& = delete;
    virtual ~PendingSetSystemsBase() = default;
    virtual auto attach(App& app) -> void = 0;
  };

  template <typename F> struct PendingSetSystems final : PendingSetSystemsBase {
    SetList sets{};
    decay_t<F> func{};

    PendingSetSystems(SetList selector, F&& system_fn)
        : sets(std::move(selector)), func(std::forward<F>(system_fn)) {}

    auto attach(App& app) -> void override {
      auto label = app.resolve_set_schedule(sets);
      app.schedules_.entry(label).add_system_fn(
        std::move(sets), std::move(func)
      );
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
  auto add_systems(ScheduleLabel label, F&& func) -> App& {
    schedules_.entry(label).add_system_fn(std::forward<F>(func));
    return *this;
  }

  template <typename F>
    requires(!SystemInput<F>)
  auto add_systems(ScheduleLabel /*label*/, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename E, typename F>
    requires(
      std::is_enum_v<bare_t<E>> && !std::same_as<bare_t<E>, ScheduleLabel> &&
      SystemInput<F>
    )
  auto add_systems(E set_id, F&& func) -> App& {
    return add_systems(set(set_id), std::forward<F>(func));
  }

  template <typename E, typename F>
    requires(
      std::is_enum_v<bare_t<E>> && !std::same_as<bare_t<E>, ScheduleLabel> &&
      !SystemInput<F>
    )
  auto add_systems(E /*set_id*/, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && SystemInput<F>)
  auto add_systems(S&&, F&& func) -> App& {
    return add_systems(set<bare_t<S>>(), std::forward<F>(func));
  }

  template <typename S, typename F>
    requires(detail::is_set_marker_v<S> && !SystemInput<F>)
  auto add_systems(S&&, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename S, typename F>
    requires(
      (std::same_as<bare_t<S>, SetId> || std::same_as<bare_t<S>, SetList>) &&
      SystemInput<F>
    )
  auto add_systems(S&& selector, F&& func) -> App& {
    auto sets = SetList{detail::to_set_ids(std::forward<S>(selector))};
    detail::ensure_unique_sets(sets.ids, "add_systems(...)");

    if (auto label = try_resolve_set_schedule(sets)) {
      schedules_.entry(*label).add_system_fn(
        std::move(sets), std::forward<F>(func)
      );
      return *this;
    }

    pending_set_systems_.push_back(
      std::make_unique<PendingSetSystems<F>>(
        std::move(sets), std::forward<F>(func)
      )
    );
    return *this;
  }

  template <typename S, typename F>
    requires(
      (std::same_as<bare_t<S>, SetId> || std::same_as<bare_t<S>, SetList>) &&
      !SystemInput<F>
    )
  auto add_systems(S&& /*selector*/, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  auto configure_set(ScheduleLabel label, SetConfigDescriptor&& desc) -> App& {
    register_set_schedule(label, desc.id());
    schedules_.entry(label).add_set_config(std::move(desc).take());
    return *this;
  }

  auto configure_set(ScheduleLabel label, ChainedSetConfigDescriptor&& desc)
    -> App& {
    auto configs = std::move(desc).take();
    for (auto const& config : configs) {
      register_set_schedule(label, config.id);
    }
    for (auto& config : configs) {
      schedules_.entry(label).add_set_config(std::move(config));
    }
    return *this;
  }

private:
  template <typename S>
  auto get_or_init_state_schedules() -> StateSchedules<S>& {
    auto* schedules = world_.get_resource<StateSchedules<S>>();
    if (!schedules) {
      world_.insert_resource(StateSchedules<S>{});
      schedules = world_.get_resource<StateSchedules<S>>();
    }
    return *schedules;
  }

public:
  template <OnEnterLabel Label, typename F>
    requires SystemInput<F>
  auto add_systems(Label&& label, F&& func) -> App& {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_enter[label.value].add_system_fn(
      std::forward<F>(func)
    );
    return *this;
  }

  template <OnEnterLabel Label, typename F>
    requires(!SystemInput<F>)
  auto add_systems(Label&& /*label*/, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <OnExitLabel Label, typename F>
    requires SystemInput<F>
  auto add_systems(Label&& label, F&& func) -> App& {
    using S = bare_t<decltype(label.value)>;
    get_or_init_state_schedules<S>().on_exit[label.value].add_system_fn(
      std::forward<F>(func)
    );
    return *this;
  }

  template <OnExitLabel Label, typename F>
    requires(!SystemInput<F>)
  auto add_systems(Label&& /*label*/, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <OnTransitionLabel Label, typename F>
    requires SystemInput<F>
  auto add_systems(Label&&, F&& func) -> App& {
    using S = typename bare_t<Label>::state_type;
    get_or_init_state_schedules<S>().on_transition.add_system_fn(
      std::forward<F>(func)
    );
    return *this;
  }

  template <OnTransitionLabel Label, typename F>
    requires(!SystemInput<F>)
  auto add_systems(Label&&, F&& /*func*/) -> App& {
    detail::static_assert_valid_system_callable<F>();
    return *this;
  }

  template <typename P>
    requires detail::BuildPlugin<P>
  auto add_plugin(P&& plugin) -> App& {
    std::forward<P>(plugin).build(*this);
    return *this;
  }

  template <typename F>
    requires(!detail::BuildPlugin<F> && detail::CallablePlugin<F>)
  auto add_plugin(F&& plugin_fn) -> App& {
    std::forward<F>(plugin_fn)(*this);
    return *this;
  }

  auto add_plugin(IPlugin& plugin) -> App& {
    plugin.build(*this);
    return *this;
  }

  template <typename R> auto insert_resource(R resource) -> App& {
    world_.insert_resource(std::move(resource));
    return *this;
  }

  template <typename R, typename... Args>
  auto init_resource(Args&&... args) -> App& {
    if (!world_.has_resource<R>()) {
      world_.insert_resource(R{std::forward<Args>(args)...});
    }
    return *this;
  }

  template <typename S> auto init_state(S initial) -> App& {
    world_.insert_resource(State<S>(std::move(initial)));
    world_.insert_resource(NextState<S>{});
    world_.insert_resource(StateTransitionEvent<S>{});
    world_.insert_resource(StateSchedules<S>{});
    schedules_.entry(ScheduleLabel::PreUpdate)
      .add_system(make_system([](World& world_ref) -> auto {
        check_state_transitions<S>(world_ref);
      }));

    S initial_copy = initial;
    schedules_.entry(ScheduleLabel::Startup)
      .add_system(make_system([initial_copy](World& world_ref) -> auto {
        auto* schedules = world_ref.get_resource<StateSchedules<S>>();
        if (schedules) {
          auto iter = schedules->on_enter.find(initial_copy);
          if (iter != schedules->on_enter.end()) {
            iter->second.run(world_ref);
          }
        }
      }));
    return *this;
  }

  template <IsMessage E> auto add_message() -> App& {
    if (!world_.has_resource<Messages<E>>()) {
      world_.insert_resource(Messages<E>{});
      schedules_.entry(ScheduleLabel::First)
        .add_system_fn(message_update_system<E>);
    }
    return *this;
  }

  template <IsEvent E> auto add_event() -> App& {
    world_.add_event<E>();
    return *this;
  }

  template <typename F> auto add_observer(F&& func) -> App& {
    world_.add_observer(std::forward<F>(func));
    return *this;
  }

  auto world() -> World& { return world_; }
  auto world() const -> World const& { return world_; }
  auto schedules() -> Schedules& { return schedules_; }

  auto startup() -> void {
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

  auto update(float delta_seconds = 0.0F) -> void {
    flush_pending_set_systems();
    debug_dump_schedule_graph_once();
    auto* time = world_.get_resource<Time>();
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

  auto run() -> void {
    auto* runner = world_.get_resource<AppRunner>();
    if ((runner == nullptr) || !runner->run) {
      std::println(
        stderr, "App::run() called without an AppRunner resource.\n"
      );
      return;
    }
    runner->run(*this);
  }

private:
  auto register_set_schedule(ScheduleLabel label, SetId const& set_id) -> void {
    auto [iter, inserted] = set_schedules_.emplace(set_id, label);
    if (!inserted && iter->second != label) {
      throw std::runtime_error(
        "Set '" + set_name(set_id) + "' is configured for both schedules '" +
        std::string(schedule_label_name(iter->second)) + "' and '" +
        std::string(schedule_label_name(label)) + "'"
      );
    }
  }

  auto try_resolve_set_schedule(SetList const& sets) const
    -> std::optional<ScheduleLabel> {
    std::optional<ScheduleLabel> label;

    for (auto const& set_id : sets.ids) {
      auto iter = set_schedules_.find(set_id);
      if (iter == set_schedules_.end()) {
        return std::nullopt;
      }

      if (label && *label != iter->second) {
        throw std::runtime_error(
          "Sets '" + detail::set_names(sets.ids) +
          "' are configured for different schedules"
        );
      }
      label = iter->second;
    }

    return label;
  }

  auto resolve_set_schedule(SetList const& sets) const -> ScheduleLabel {
    auto label = try_resolve_set_schedule(sets);
    if (!label) {
      throw std::runtime_error(
        "Sets '" + detail::set_names(sets.ids) +
        "' were used by add_systems(...) but were not "
        "configured"
      );
    }
    return *label;
  }

  auto flush_pending_set_systems() -> void {
    if (pending_set_systems_.empty()) {
      return;
    }

    auto pending = std::move(pending_set_systems_);
    pending_set_systems_.clear();
    for (auto& entry : pending) {
      entry->attach(*this);
    }
  }

  auto debug_dump_schedule_graph_once() -> void {
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

  auto apply_commands() -> void {
    auto* queues = world_.get_resource<CommandQueues>();
    if ((queues != nullptr) && !queues->empty()) {
      world_.increment_change_tick();
      queues->apply(world_);
    }
  }
};

inline auto time_plugin(App& app) -> void { app.init_resource<Time>(); }

inline auto Window::install(Window config) {
  return [config = std::move(config)](ecs::App& app) -> void {
    app.insert_resource(config);
  };
}

inline auto window_plugin(Window config) {
  return Window::install(std::move(config));
}
} // namespace ecs
