#pragma once
#include "types.hpp"
#include <cstdint>

namespace ecs {

struct Tick {
  u32 value{};

  Tick() = default;
  explicit Tick(u32 v) : value(v) {}

  [[nodiscard]] bool is_newer_than(Tick last_run, Tick this_run) const {
    u32 ticks_since_last_run = this_run.value - last_run.value;
    u32 ticks_since_self = this_run.value - value;
    return ticks_since_self < ticks_since_last_run;
  }
};

struct ComponentTicks {
  Tick added{};
  Tick changed{};
  Tick this_run{};
  Tick last_run{};
};

struct SystemContext {
  Tick last_run{};
  Tick this_run{};
};

} // namespace ecs
