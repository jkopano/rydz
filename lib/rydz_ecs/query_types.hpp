#pragma once
#include "ticks.hpp"

namespace ecs {

template <typename T> struct Mut {
  T *ptr{};
  ComponentTicks *ticks{};
  Tick tick{};
  bool marked{};

  Mut() = default;
  Mut(T *p, ComponentTicks *t, Tick current_tick)
      : ptr(p), ticks(t), tick(current_tick) {}

  auto get() -> T & {
    mark();
    return *ptr;
  }
  auto get() const -> const T & { return *ptr; }

  auto operator->() -> T * {
    mark();
    return ptr;
  }
  auto operator->() const -> const T * { return ptr; }

  auto operator*() -> T & {
    mark();
    return *ptr;
  }
  auto operator*() const -> const T & { return *ptr; }

  explicit operator bool() const { return ptr != nullptr; }

  operator T *() {
    mark();
    return ptr;
  }
  operator const T *() const { return ptr; }

  auto bypass_change_detection() -> T & { return *ptr; }

private:
  auto mark() -> void {
    if (!marked && (ticks != nullptr)) {
      ticks->changed = tick;
      marked = true;
    }
  }
};

template <typename Inner> struct Opt {};

} // namespace ecs
