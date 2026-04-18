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
  T* ptr{};

  auto operator*() -> T& { return *ptr; }
  auto operator->() -> T* { return ptr; }
  auto operator*() const -> T const& { return *ptr; }
  auto operator->() const -> T const* { return ptr; }
};

template <typename T> struct Res {
  T const* ptr{};

  auto operator*() const -> T const& { return *ptr; }
  auto operator->() const -> T const* { return ptr; }
};

template <typename T> struct ResMut {
  T* ptr{};

  auto operator*() -> T& { return *ptr; }
  auto operator->() -> T* { return ptr; }
  auto operator*() const -> T const& { return *ptr; }
  auto operator->() const -> T const* { return ptr; }
};

template <typename E> class MessageWriter {
  Messages<E>* messages_;

public:
  explicit MessageWriter(Messages<E>* messages) : messages_(messages) {}

  auto send(E const& message) -> void { messages_->send(message); }
  auto send(E&& message) -> void { messages_->send(std::move(message)); }
};

template <typename E> class MessageReader {
  Messages<E>* messages_;
  usize reader_id_;

public:
  explicit MessageReader(Messages<E>* messages)
      : messages_(messages), reader_id_(messages->register_reader()) {}

  auto iter() { return messages_->iter(reader_id_); }

  [[nodiscard]] auto is_empty() const -> bool {
    return !messages_->has_unread(reader_id_);
  }
};

template <typename P> struct DefaultSystemParamState {
  using State = std::monostate;
  static auto init_state(World&) -> State { return {}; }
};

template <typename P> struct SystemParamTraits : DefaultSystemParamState<P> {};

template <typename T>
struct SystemParamTraits<Res<T>> : DefaultSystemParamState<Res<T>> {
  using Item = Res<T>;

  static auto retrieve(World& world, SystemContext const&) -> Item {
    T const* ptr = world.get_resource<T>();
    if (ptr == nullptr) {
      throw std::runtime_error(
        std::string("Resource not found: ") + typeid(T).name()
      );
    }
    return Res<T>{ptr};
  }

  static auto available(World const& world) -> bool {
    return world.has_resource<T>();
  }
  static auto access(SystemAccess& acc) -> void { acc.add_resource_read<T>(); }
};

template <typename T>
struct SystemParamTraits<ResMut<T>> : DefaultSystemParamState<ResMut<T>> {
  using Item = ResMut<T>;

  static auto retrieve(World& world, SystemContext const&) -> Item {
    T* ptr = world.get_resource<T>();
    if (ptr == nullptr) {
      throw std::runtime_error(
        std::string("Resource not found: ") + typeid(T).name()
      );
    }
    return ResMut<T>{ptr};
  }

  static auto available(World const& world) -> bool {
    return world.has_resource<T>();
  }
  static auto access(SystemAccess& acc) -> void { acc.add_resource_write<T>(); }
};

template <> struct SystemParamTraits<World> : DefaultSystemParamState<World> {
  using Item = World&;

  static auto access(SystemAccess& acc) -> void {
    acc.set_exclusive();
    acc.set_main_thread_only();
  }

  static auto retrieve(World& world, SystemContext const&) -> Item {
    return world;
  }
  static auto available(World const&) -> bool { return true; }
};

template <>
struct SystemParamTraits<NonSendMarker>
    : DefaultSystemParamState<NonSendMarker> {
  using Item = NonSendMarker;

  static auto access(SystemAccess& acc) -> void {
    acc.set_exclusive();
    acc.set_main_thread_only();
  }

  static auto retrieve(World&, SystemContext const&) -> Item { return {}; }
  static auto available(World const&) -> bool { return true; }
};

template <typename E>
struct SystemParamTraits<MessageWriter<E>>
    : DefaultSystemParamState<MessageWriter<E>> {
  using Item = MessageWriter<E>;

  static auto retrieve(World& world, SystemContext const&) -> Item {
    auto* messages = world.get_resource<Messages<E>>();
    if (messages == nullptr) {
      throw std::runtime_error("Messages resource not found");
    }
    return MessageWriter<E>(messages);
  }

  static auto available(World const& world) -> bool {
    return world.has_resource<Messages<E>>();
  }

  static auto access(SystemAccess& acc) -> void {
    acc.add_resource_write<Messages<E>>();
  }
};

template <typename E>
struct SystemParamTraits<MessageReader<E>>
    : DefaultSystemParamState<MessageReader<E>> {
  using Item = MessageReader<E>;

  static auto retrieve(World& world, SystemContext const&) -> Item {
    auto* messages = world.get_resource<Messages<E>>();
    if (messages == nullptr) {
      throw std::runtime_error("Messages resource not found");
    }
    return MessageReader<E>(messages);
  }

  static auto available(World const& world) -> bool {
    return world.has_resource<Messages<E>>();
  }

  static auto access(SystemAccess& acc) -> void {
    acc.add_resource_read<Messages<E>>();
  }
};

template <typename T> struct SystemParamTraits<Local<T>> {
  using Item = Local<T>;
  using State = T;

  static auto init_state(World&) -> State {
    static_assert(
      std::default_initializable<T>,
      "Local<T> requires T to be default-initializable"
    );
    return T{};
  }

  static auto retrieve(World&, SystemContext const&, State& state) -> Item {
    return Local<T>{&state};
  }

  static auto available(World const&) -> bool { return true; }
  static auto access(SystemAccess&) -> void {}
};

template <typename E> void message_update_system(ResMut<Messages<E>> messages) {
  messages->update();
}

} // namespace ecs
