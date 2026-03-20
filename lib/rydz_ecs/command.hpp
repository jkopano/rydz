#pragma once
#include "bundle.hpp"
#include "entity.hpp"
#include "system.hpp"
#include "world.hpp"
#include <functional>
#include <memory>
#include <ranges>
#include <vector>

namespace ecs {

class ICommand {
public:
  virtual ~ICommand() = default;
  virtual void apply(World &world) = 0;
};

class CommandQueue {
public:
  using Type = ResourceType;

private:
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

template <typename T> struct InsertCommand : ICommand {
  Entity entity;
  T item;
  InsertCommand(Entity e, T i) : entity(e), item(std::move(i)) {}
  void apply(World &world) override {
    insert_bundle(world, entity, std::move(item));
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

  template <Spawnable T> EntityCommands &insert(T item) {
    queue_->push(detail::InsertCommand<T>(entity_, std::move(item)));
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

  template <Spawnable... Ts> EntityCommands spawn(Ts... items) {
    Entity entity = entities_->create();
    (queue_->push(detail::InsertCommand<Ts>(entity, std::move(items))), ...);
    return EntityCommands(entity, queue_, entities_);
  }

  EntityCommands spawn_empty() {
    Entity entity = entities_->create();
    return EntityCommands(entity, queue_, entities_);
  }

  template <std::ranges::input_range R>
    requires Spawnable<std::ranges::range_value_t<R>>
  void spawn_batch(R &&_range) {
    for (auto &&item : std::forward<R>(_range)) {
      spawn(std::forward<decltype(item)>(item));
    }
  }

  void despawn(Entity entity) { queue_->push(detail::DespawnCommand(entity)); }

  template <typename T> void insert_resource(T resource) {
    static_assert(!Bundle<T>, "Bundles cannot be inserted as resources.");
    static_assert(Resource<T>,
                  "Only Resources can be inserted via insert_resource(). "
                  "Add 'using Type = ResourceType;' to your struct.");
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
