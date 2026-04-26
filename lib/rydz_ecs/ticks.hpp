#pragma once
#include "types.hpp"
#include <cstdint>

namespace ecs {

struct Tick {
  u32 value{};

  Tick() = default;
  explicit Tick(u32 v) : value(v) {}

  [[nodiscard]] auto is_newer_than(Tick last_run, Tick this_run) const -> bool {
    u32 ticks_since_last_run = this_run.value - last_run.value;
    u32 ticks_since_self = this_run.value - value;
    return ticks_since_self < ticks_since_last_run;
  }

  // Inclusive variant: detects changes at the same tick as last_run.
  // Used by Changed<T> to make detection order-independent.
  [[nodiscard]] auto is_newer_or_equal(Tick last_run, Tick this_run) const -> bool {
    u32 ticks_since_last_run = this_run.value - last_run.value;
    u32 ticks_since_self = this_run.value - value;
    return ticks_since_self <= ticks_since_last_run;
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
