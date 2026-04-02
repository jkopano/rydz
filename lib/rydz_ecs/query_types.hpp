#pragma once
#include "ticks.hpp"

namespace ecs {

template <typename T> struct Mut {
  T *ptr = nullptr;
  ComponentTicks *ticks = nullptr;
  Tick tick{};
  bool marked = false;

  Mut() = default;
  Mut(T *p, ComponentTicks *t, Tick current_tick)
      : ptr(p), ticks(t), tick(current_tick) {}

  T &get() {
    mark();
    return *ptr;
  }
  const T &get() const { return *ptr; }

  T *operator->() {
    mark();
    return ptr;
  }
  const T *operator->() const { return ptr; }

  T &operator*() {
    mark();
    return *ptr;
  }
  const T &operator*() const { return *ptr; }

  explicit operator bool() const { return ptr != nullptr; }

  operator T *() {
    mark();
    return ptr;
  }
  operator const T *() const { return ptr; }

  T &bypass_change_detection() { return *ptr; }

private:
  void mark() {
    if (!marked && ticks) {
      ticks->changed = tick;
      marked = true;
    }
  }
};

template <typename Inner> struct Opt {};

} // namespace ecs
