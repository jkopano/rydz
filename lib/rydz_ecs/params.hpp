#pragma once
#include "access.hpp"
#include "event.hpp"
#include "fwd.hpp"
#include "world.hpp"
#include <concepts>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <variant>

namespace ecs {

struct NonSendMarker {};

template <typename T> struct Local {
  T *ptr = nullptr;

  T &operator*() { return *ptr; }
  T *operator->() { return ptr; }
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }
};

template <typename T> struct Res {
  const T *ptr = nullptr;

  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }
};

template <typename T> struct ResMut {
  T *ptr = nullptr;

  T &operator*() { return *ptr; }
  T *operator->() { return ptr; }
  const T &operator*() const { return *ptr; }
  const T *operator->() const { return ptr; }
};

template <typename E> class EventWriter {
  Events<E> *events_;

public:
  explicit EventWriter(Events<E> *events) : events_(events) {}

  void send(const E &event) { events_->send(event); }
  void send(E &&event) { events_->send(std::move(event)); }
};

template <typename E> class EventReader {
  Events<E> *events_;
  usize reader_id_;

public:
  explicit EventReader(Events<E> *events)
      : events_(events), reader_id_(events->register_reader()) {}

  template <typename Func> void for_each(Func &&func) {
    events_->read(reader_id_, std::forward<Func>(func));
  }

  bool is_empty() const { return !events_->has_unread(reader_id_); }
};

template <typename P> struct DefaultSystemParamState {
  using State = std::monostate;
  static State init_state(World &) { return {}; }
};

template <typename P> struct SystemParamTraits : DefaultSystemParamState<P> {};

template <typename T>
struct SystemParamTraits<Res<T>> : DefaultSystemParamState<Res<T>> {
  using Item = Res<T>;

  static Item retrieve(World &world, const SystemContext &) {
    const T *ptr = world.get_resource<T>();
    if (!ptr) {
      throw std::runtime_error(std::string("Resource not found: ") +
                               typeid(T).name());
    }
    return Res<T>{ptr};
  }

  static bool available(const World &world) { return world.has_resource<T>(); }
  static void access(SystemAccess &acc) { acc.add_resource_read<T>(); }
};

template <typename T>
struct SystemParamTraits<ResMut<T>> : DefaultSystemParamState<ResMut<T>> {
  using Item = ResMut<T>;

  static Item retrieve(World &world, const SystemContext &) {
    T *ptr = world.get_resource<T>();
    if (!ptr) {
      throw std::runtime_error(std::string("Resource not found: ") +
                               typeid(T).name());
    }
    return ResMut<T>{ptr};
  }

  static bool available(const World &world) { return world.has_resource<T>(); }
  static void access(SystemAccess &acc) { acc.add_resource_write<T>(); }
};

template <> struct SystemParamTraits<World> : DefaultSystemParamState<World> {
  using Item = World &;

  static void access(SystemAccess &acc) {
    acc.set_exclusive();
    acc.set_main_thread_only();
  }

  static Item retrieve(World &world, const SystemContext &) { return world; }
  static bool available(const World &) { return true; }
};

template <>
struct SystemParamTraits<NonSendMarker>
    : DefaultSystemParamState<NonSendMarker> {
  using Item = NonSendMarker;

  static void access(SystemAccess &acc) { acc.set_main_thread_only(); }

  static Item retrieve(World &, const SystemContext &) { return {}; }
  static bool available(const World &) { return true; }
};

template <typename E>
struct SystemParamTraits<EventWriter<E>>
    : DefaultSystemParamState<EventWriter<E>> {
  using Item = EventWriter<E>;

  static Item retrieve(World &world, const SystemContext &) {
    auto *events = world.get_resource<Events<E>>();
    if (!events)
      throw std::runtime_error("Events resource not found");
    return EventWriter<E>(events);
  }

  static bool available(const World &world) {
    return world.has_resource<Events<E>>();
  }

  static void access(SystemAccess &acc) { acc.add_resource_write<Events<E>>(); }
};

template <typename E>
struct SystemParamTraits<EventReader<E>>
    : DefaultSystemParamState<EventReader<E>> {
  using Item = EventReader<E>;

  static Item retrieve(World &world, const SystemContext &) {
    auto *events = world.get_resource<Events<E>>();
    if (!events)
      throw std::runtime_error("Events resource not found");
    return EventReader<E>(events);
  }

  static bool available(const World &world) {
    return world.has_resource<Events<E>>();
  }

  static void access(SystemAccess &acc) { acc.add_resource_read<Events<E>>(); }
};

template <typename T> struct SystemParamTraits<Local<T>> {
  using Item = Local<T>;
  using State = T;

  static State init_state(World &) {
    static_assert(std::default_initializable<T>,
                  "Local<T> requires T to be default-initializable");
    return T{};
  }

  static Item retrieve(World &, const SystemContext &, State &state) {
    return Local<T>{&state};
  }

  static bool available(const World &) { return true; }
  static void access(SystemAccess &) {}
};

template <typename E> void event_update_system(ResMut<Events<E>> events) {
  events->update();
}

} // namespace ecs
