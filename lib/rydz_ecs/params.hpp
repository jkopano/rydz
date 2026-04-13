#pragma once
#include "access.hpp"
#include "fwd.hpp"
#include "message.hpp"
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

template <typename E> class MessageWriter {
  Messages<E> *messages_;

public:
  explicit MessageWriter(Messages<E> *messages) : messages_(messages) {}

  void send(const E &message) { messages_->send(message); }
  void send(E &&message) { messages_->send(std::move(message)); }
};

template <typename E> class MessageReader {
  Messages<E> *messages_;
  usize reader_id_;

public:
  explicit MessageReader(Messages<E> *messages)
      : messages_(messages), reader_id_(messages->register_reader()) {}

  auto iter() { return messages_->iter(reader_id_); }

  bool is_empty() const { return !messages_->has_unread(reader_id_); }
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

  static void access(SystemAccess &acc) {
    acc.set_exclusive();
    acc.set_main_thread_only();
  }

  static Item retrieve(World &, const SystemContext &) { return {}; }
  static bool available(const World &) { return true; }
};

template <typename E>
struct SystemParamTraits<MessageWriter<E>>
    : DefaultSystemParamState<MessageWriter<E>> {
  using Item = MessageWriter<E>;

  static Item retrieve(World &world, const SystemContext &) {
    auto *messages = world.get_resource<Messages<E>>();
    if (!messages)
      throw std::runtime_error("Messages resource not found");
    return MessageWriter<E>(messages);
  }

  static bool available(const World &world) {
    return world.has_resource<Messages<E>>();
  }

  static void access(SystemAccess &acc) {
    acc.add_resource_write<Messages<E>>();
  }
};

template <typename E>
struct SystemParamTraits<MessageReader<E>>
    : DefaultSystemParamState<MessageReader<E>> {
  using Item = MessageReader<E>;

  static Item retrieve(World &world, const SystemContext &) {
    auto *messages = world.get_resource<Messages<E>>();
    if (!messages)
      throw std::runtime_error("Messages resource not found");
    return MessageReader<E>(messages);
  }

  static bool available(const World &world) {
    return world.has_resource<Messages<E>>();
  }

  static void access(SystemAccess &acc) {
    acc.add_resource_read<Messages<E>>();
  }
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

template <typename E> void message_update_system(ResMut<Messages<E>> messages) {
  messages->update();
}

} // namespace ecs
