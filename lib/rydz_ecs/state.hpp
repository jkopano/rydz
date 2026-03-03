#pragma once
#include "condition.hpp"
#include "system.hpp"
#include <functional>
#include <optional>

namespace ecs {

template <typename S> struct State {
  S value;

  explicit State(S v) : value(std::move(v)) {}

  const S &get() const { return value; }
};

template <typename S> struct NextState {
  std::optional<S> pending;

  NextState() = default;

  void set(S state) { pending = std::move(state); }
  void clear() { pending = std::nullopt; }
};

template <typename S> auto in_state(S target) {
  return
      [target](Res<State<S>> state) -> bool { return state->get() == target; };
}

template <typename S> void check_state_transitions(World &world) {
  auto *next = world.get_resource<NextState<S>>();
  if (!next || !next->pending)
    return;

  auto *state = world.get_resource<State<S>>();
  if (!state)
    return;

  S new_value = std::move(*next->pending);
  next->pending = std::nullopt;

  if (!(state->value == new_value)) {
    state->value = std::move(new_value);
  }
}

} // namespace ecs
