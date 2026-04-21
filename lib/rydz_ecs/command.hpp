#pragma once
#include "bundle.hpp"
#include "entity.hpp"
#include "params.hpp"
#include "world.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <vector>

namespace ecs {

class ICommand {
public:
  ICommand() = default;
  ICommand(ICommand const&) = default;
  ICommand(ICommand&&) noexcept = default;
  auto operator=(ICommand const&) -> ICommand& = default;
  auto operator=(ICommand&&) noexcept -> ICommand& = default;
  virtual ~ICommand() = default;
  virtual auto apply(World& world) -> void = 0;
};

class CommandQueue {
  std::vector<std::unique_ptr<ICommand>> queue_;

public:
  template <typename Cmd> void push(Cmd&& cmd) {
    queue_.push_back(std::make_unique<bare_t<Cmd>>(std::forward<Cmd>(cmd)));
  }

  auto apply(World& world) -> void {
    auto commands = std::move(queue_);
    queue_.clear();
    for (auto& cmd : commands) {
      cmd->apply(world);
    }
  }

  [[nodiscard]] auto empty() const -> bool { return queue_.empty(); }
};

class CommandQueues {
public:
  using T = Resource;

  auto submit(CommandQueue queue) -> void {
    std::lock_guard lock(*mutex_);
    queues_.push_back(std::move(queue));
  }

  auto apply(World& world) -> void {
    auto queues = std::move(queues_);
    queues_.clear();
    for (auto& q : queues) {
      q.apply(world);
    }
  }

  [[nodiscard]] auto empty() const -> bool { return queues_.empty(); }

private:
  std::unique_ptr<std::mutex> mutex_ = std::make_unique<std::mutex>();
  std::vector<CommandQueue> queues_;
};

namespace detail {

struct ActivateEntityCommand : ICommand {
  Entity entity;
  explicit ActivateEntityCommand(Entity e) : entity(e) {}
  auto apply(World& world) -> void override { world.entities.activate(entity); }
};

template <typename T> struct InsertCommand : ICommand {
  Entity entity;
  T item;
  InsertCommand(Entity e, T i) : entity(e), item(std::move(i)) {}
  auto apply(World& world) -> void override {
    insert_bundle(world, entity, std::move(item));
  }
};

struct DespawnCommand : ICommand {
  Entity entity;
  DespawnCommand(Entity e) : entity(e) {}
  auto apply(World& world) -> void override { world.despawn(entity); }
};

template <typename T> struct RemoveComponentCommand : ICommand {
  Entity entity;
  RemoveComponentCommand(Entity e) : entity(e) {}
  auto apply(World& world) -> void override {
    world.remove_component<T>(entity);
  }
};

template <typename T> struct InsertResourceCommand : ICommand {
  T resource;
  InsertResourceCommand(T r) : resource(std::move(r)) {}
  auto apply(World& world) -> void override {
    world.insert_resource(std::move(resource));
  }
};

template <typename F> struct AddObserverCommand : ICommand {
  F func;
  AddObserverCommand(F f) : func(std::move(f)) {}
  auto apply(World& world) -> void override {
    world.add_observer(std::move(func));
  }
};

template <typename F> struct AddEntityObserverCommand : ICommand {
  Entity target;
  F func;
  AddEntityObserverCommand(Entity target, F f)
      : target(target), func(std::move(f)) {}
  auto apply(World& world) -> void override {
    world.add_observer(target, std::move(func));
  }
};

template <typename E> struct TriggerCommand : ICommand {
  E event;
  TriggerCommand(E event) : event(std::move(event)) {}
  auto apply(World& world) -> void override { world.trigger(std::move(event)); }
};

} // namespace detail

class EntityCommands {
  Entity entity_;
  CommandQueue* queue_;

public:
  EntityCommands(Entity entity, CommandQueue* queue)
      : entity_(entity), queue_(queue) {}

  template <Spawnable T> auto insert(T item) -> EntityCommands& {
    queue_->push(detail::InsertCommand<T>(entity_, std::move(item)));
    return *this;
  }

  template <typename T> auto remove() -> EntityCommands& {
    queue_->push(detail::RemoveComponentCommand<T>(entity_));
    return *this;
  }

  template <typename F> auto observe(F&& func) -> EntityCommands& {
    using Fn = decay_t<F>;
    using EventT = detail::observer_event_t<Fn>;
    static_assert(
      IsEntityEvent<EventT>,
      "EntityCommands::observe requires On<E> where E is an "
      "EntityEvent."
    );
    queue_->push(
      detail::AddEntityObserverCommand<Fn>(entity_, std::forward<F>(func))
    );
    return *this;
  }

  auto despawn() -> void { queue_->push(detail::DespawnCommand(entity_)); }

  auto id() const -> Entity { return entity_; }
};

class Cmd {
  CommandQueue queue_;
  CommandQueues* parent_;
  EntityManager* entities_;

public:
  Cmd(CommandQueues* parent, EntityManager* entities)
      : parent_(parent), entities_(entities) {}

  ~Cmd() {
    if ((parent_ != nullptr) && !queue_.empty()) {
      parent_->submit(std::move(queue_));
    }
  }

  Cmd(Cmd&&) = default;
  auto operator=(Cmd&&) -> Cmd& = default;
  Cmd(Cmd const&) = delete;
  auto operator=(Cmd const&) -> Cmd& = delete;

  template <Spawnable... Ts> auto spawn(Ts... items) -> EntityCommands {
    Entity entity = entities_->reserve();
    queue_.push(detail::ActivateEntityCommand(entity));
    (queue_.push(detail::InsertCommand<Ts>(entity, std::move(items))), ...);
    return {entity, &queue_};
  }

  auto spawn_empty() -> EntityCommands {
    Entity entity = entities_->reserve();
    queue_.push(detail::ActivateEntityCommand(entity));
    return {entity, &queue_};
  }

  auto spawn_batch(SpawnableRange auto&& _range) -> void {
    for (auto&& item : _range) {
      spawn(std::forward<decltype(item)>(item));
    }
  }

  auto despawn(Entity entity) -> void {
    queue_.push(detail::DespawnCommand(entity));
  }

  template <typename T> void insert_resource(T resource) {
    queue_.push(detail::InsertResourceCommand<T>(std::move(resource)));
  }

  template <typename F> auto add_observer(F&& func) -> Cmd& {
    queue_.push(detail::AddObserverCommand<decay_t<F>>(std::forward<F>(func)));
    return *this;
  }

  template <typename E>
    requires IsEvent<bare_t<E>>
  auto trigger(E&& event) -> Cmd& {
    queue_.push(detail::TriggerCommand<bare_t<E>>(std::forward<E>(event)));
    return *this;
  }

  auto entity(Entity e) -> EntityCommands { return EntityCommands(e, &queue_); }
};

template <> struct SystemParamTraits<Cmd> : DefaultSystemParamState<Cmd> {
  using Item = Cmd;

  static auto retrieve(World& world, SystemContext const&) -> Item {
    auto* queues = world.get_resource<CommandQueues>();
    if (queues == nullptr) {
      throw std::runtime_error("CommandQueues resource not found");
    }

    return {queues, &world.entities};
  }

  static auto available(World const& world) -> bool {
    return world.has_resource<CommandQueues>();
  }

  static auto access(SystemAccess& acc) -> void {
    acc.add_resource_read<CommandQueues>();
  }
};

} // namespace ecs
