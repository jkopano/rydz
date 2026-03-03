#pragma once
#include <cstdint>

namespace ecs {

struct Tick {
  uint32_t value = 0;

  Tick() = default;
  explicit Tick(uint32_t v) : value(v) {}

  bool is_newer_than(Tick last_run, Tick this_run) const {
    uint32_t ticks_since_last_run = this_run.value - last_run.value;
    uint32_t ticks_since_self = this_run.value - value;
    return ticks_since_self < ticks_since_last_run;
  }
};

struct ComponentTicks {
  Tick added{};
  Tick changed{};
  Tick this_run{};
  Tick last_run{};
};

template <typename T> struct ComponentData {
  T component;
  ComponentTicks ticks;
};

} // namespace ecs
