#pragma once
#include "fwd.hpp"
#include "schedule.hpp"
#include "system.hpp"
#include <absl/container/flat_hash_map.h>
#include <optional>

namespace ecs {

template <typename S> struct State {
  using Type = ResourceType;
  S value;

  explicit State(S v) : value(std::move(v)) {}

  const S &get() const { return value; }
};

template <typename S> struct NextState {
  using Type = ResourceType;
  std::optional<S> pending;

  NextState() = default;

  void set(S state) { pending = std::move(state); }
  void clear() { pending = std::nullopt; }
};

template <typename S> struct OnEnter {
  S value;
  explicit OnEnter(S v) : value(std::move(v)) {}
};

template <typename S> struct OnExit {
  S value;
  explicit OnExit(S v) : value(std::move(v)) {}
};

template <typename S> struct OnTransition {
  using state_type = S;
};

template <typename S> struct StateTransitionEvent {
  using Type = ResourceType;
  bool changed = false;
};

template <typename S> struct StateSchedules {
  using Type = ResourceType;
  absl::flat_hash_map<S, Schedule> on_enter;
  absl::flat_hash_map<S, Schedule> on_exit;
  Schedule on_transition;
};

template <typename T> struct is_on_enter : std::false_type {};
template <typename S> struct is_on_enter<OnEnter<S>> : std::true_type {};

template <typename T> struct is_on_exit : std::false_type {};
template <typename S> struct is_on_exit<OnExit<S>> : std::true_type {};

template <typename T> struct is_on_transition : std::false_type {};
template <typename S>
struct is_on_transition<OnTransition<S>> : std::true_type {};

template <typename T>
concept OnEnterLabel = is_on_enter<std::decay_t<T>>::value;

template <typename T>
concept OnExitLabel = is_on_exit<std::decay_t<T>>::value;

template <typename T>
concept OnTransitionLabel = is_on_transition<std::decay_t<T>>::value;

template <typename S> auto in_state(S target) {
  return
      [target](Res<State<S>> state) -> bool { return state->get() == target; };
}

template <typename S> auto state_changed() {
  return
      [](Res<StateTransitionEvent<S>> event) -> bool { return event->changed; };
}

template <typename S> void check_state_transitions(World &world) {
  auto *event = world.get_resource<StateTransitionEvent<S>>();
  if (event)
    event->changed = false;

  auto *next = world.get_resource<NextState<S>>();
  if (!next || !next->pending)
    return;

  auto *state = world.get_resource<State<S>>();
  if (!state)
    return;

  S new_value = std::move(*next->pending);
  next->pending = std::nullopt;

  if (!(state->value == new_value)) {
    S old_value = std::move(state->value);

    auto *schedules = world.get_resource<StateSchedules<S>>();
    if (schedules) {
      auto exit_it = schedules->on_exit.find(old_value);
      if (exit_it != schedules->on_exit.end())
        exit_it->second.run(world);
    }

    state->value = std::move(new_value);

    if (schedules) {
      schedules->on_transition.run(world);

      auto enter_it = schedules->on_enter.find(state->value);
      if (enter_it != schedules->on_enter.end())
        enter_it->second.run(world);
    }

    if (event)
      event->changed = true;
  }
}

} // namespace ecs
