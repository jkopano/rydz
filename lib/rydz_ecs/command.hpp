#pragma once
#include "bundle.hpp"
#include "entity.hpp"
#include "system.hpp"
#include "world.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace ecs {

class ICommand {
public:
  virtual ~ICommand() = default;
  virtual void apply(World &world) = 0;
};

class CommandQueue {
  std::vector<std::shared_ptr<ICommand>> queue_;

public:
  template <typename Cmd> void push(Cmd &&cmd) {
    queue_.push_back(
        std::make_shared<std::decay_t<Cmd>>(std::forward<Cmd>(cmd)));
  }

  void apply(World &world) {
    auto commands = std::move(queue_);
    queue_.clear();
    for (auto &cmd : commands) {
      cmd->apply(world);
    }
  }

  bool empty() const { return queue_.empty(); }
};

namespace detail {

template <typename T> struct SpawnComponentCommand : ICommand {
  Entity entity;
  T component;
  SpawnComponentCommand(Entity e, T c) : entity(e), component(std::move(c)) {}
  void apply(World &world) override {
    world.insert_component(entity, std::move(component));
  }
};

template <typename... Ts> struct SpawnBundleCommand : ICommand {
  Entity entity;
  std::tuple<Ts...> bundle;
  SpawnBundleCommand(Entity e, std::tuple<Ts...> b)
      : entity(e), bundle(std::move(b)) {}
  void apply(World &world) override {
    insert_bundle(world, entity, std::move(bundle));
  }
};

struct DespawnCommand : ICommand {
  Entity entity;
  DespawnCommand(Entity e) : entity(e) {}
  void apply(World &world) override { world.despawn(entity); }
};

template <typename T> struct RemoveComponentCommand : ICommand {
  Entity entity;
  RemoveComponentCommand(Entity e) : entity(e) {}
  void apply(World &world) override { world.remove_component<T>(entity); }
};

template <typename T> struct InsertResourceCommand : ICommand {
  T resource;
  InsertResourceCommand(T r) : resource(std::move(r)) {}
  void apply(World &world) override {
    world.insert_resource(std::move(resource));
  }
};

} // namespace detail

class EntityCommands {
  Entity entity_;
  CommandQueue *queue_;
  EntityManager *entities_;

public:
  EntityCommands(Entity entity, CommandQueue *queue, EntityManager *entities)
      : entity_(entity), queue_(queue), entities_(entities) {}

  template <typename T> EntityCommands &insert(T component) {
    queue_->push(
        detail::SpawnComponentCommand<T>(entity_, std::move(component)));
    return *this;
  }

  template <typename... Ts>
  EntityCommands &insert_bundle(std::tuple<Ts...> bundle) {
    queue_->push(detail::SpawnBundleCommand<Ts...>(entity_, std::move(bundle)));
    return *this;
  }

  template <typename T> EntityCommands &remove() {
    queue_->push(detail::RemoveComponentCommand<T>(entity_));
    return *this;
  }

  void despawn() { queue_->push(detail::DespawnCommand(entity_)); }

  Entity id() const { return entity_; }
};

class Cmd {
  CommandQueue *queue_;
  EntityManager *entities_;

public:
  Cmd(CommandQueue *queue, EntityManager *entities)
      : queue_(queue), entities_(entities) {}

  template <typename T> EntityCommands spawn(T component) {
    Entity entity = entities_->create();
    queue_->push(
        detail::SpawnComponentCommand<T>(entity, std::move(component)));
    return EntityCommands(entity, queue_, entities_);
  }

  template <typename... Ts> EntityCommands spawn(Ts... components) {
    Entity entity = entities_->create();

    (queue_->push(
         detail::SpawnComponentCommand<Ts>(entity, std::move(components))),
     ...);
    return EntityCommands(entity, queue_, entities_);
  }

  EntityCommands spawn_empty() {
    Entity entity = entities_->create();
    return EntityCommands(entity, queue_, entities_);
  }

  void despawn(Entity entity) { queue_->push(detail::DespawnCommand(entity)); }

  template <typename T> void insert_resource(T resource) {
    queue_->push(detail::InsertResourceCommand<T>(std::move(resource)));
  }

  EntityCommands entity(Entity e) {
    return EntityCommands(e, queue_, entities_);
  }
};

template <> struct SystemParamTraits<Cmd> {
  using Item = Cmd;

  static Item retrieve(World &world) {
    auto *queue = world.get_resource<CommandQueue>();
    if (!queue) {
      throw std::runtime_error("CommandQueue resource not found");
    }
    return Cmd(queue, &world.entities);
  }

  static bool available(const World &world) {
    return world.has_resource<CommandQueue>();
  }

  static void access(SystemAccess &acc) {
    acc.add_resource_read<CommandQueue>();
  }
};

} // namespace ecs
