#pragma once
#include "bundle.hpp"
#include "entity.hpp"
#include "system.hpp"
#include "world.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
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

class CommandQueues {
public:
  using Type = Resource;

  void submit(CommandQueue queue) {
    std::lock_guard lock(*mutex_);
    queues_.push_back(std::move(queue));
  }

  void apply(World &world) {
    for (auto &q : queues_) {
      q.apply(world);
    }
    queues_.clear();
  }

  bool empty() const { return queues_.empty(); }

private:
  std::unique_ptr<std::mutex> mutex_ = std::make_unique<std::mutex>();
  std::vector<CommandQueue> queues_;
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

public:
  EntityCommands(Entity entity, CommandQueue *queue)
      : entity_(entity), queue_(queue) {}

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
  CommandQueue queue_;
  CommandQueues *parent_;
  EntityManager *entities_;

public:
  Cmd(CommandQueues *parent, EntityManager *entities)
      : parent_(parent), entities_(entities) {}

  ~Cmd() {
    if (parent_ && !queue_.empty()) {
      parent_->submit(std::move(queue_));
    }
  }

  Cmd(Cmd &&) = default;
  Cmd &operator=(Cmd &&) = default;
  Cmd(const Cmd &) = delete;
  Cmd &operator=(const Cmd &) = delete;

  template <Spawnable... Ts> EntityCommands spawn(Ts... items) {
    Entity entity = entities_->create();
    (queue_.push(detail::InsertCommand<Ts>(entity, std::move(items))), ...);
    return EntityCommands(entity, &queue_);
  }

  EntityCommands spawn_empty() {
    Entity entity = entities_->create();
    return EntityCommands(entity, &queue_);
  }

  void spawn_batch(SpawnableRange auto &&_range) {
    for (auto &&item : _range) {
      spawn(std::forward<decltype(item)>(item));
    }
  }

  void despawn(Entity entity) { queue_.push(detail::DespawnCommand(entity)); }

  template <typename T> void insert_resource(T resource) {
    static_assert(ResourceTrait<T>,
                  "Only Resources can be inserted via insert_resource(). "
                  "Add 'using Type = Resource;' to your struct.");
    queue_.push(detail::InsertResourceCommand<T>(std::move(resource)));
  }

  EntityCommands entity(Entity e) { return EntityCommands(e, &queue_); }
};

template <> struct SystemParamTraits<Cmd> {
  using Item = Cmd;

  static Item retrieve(World &world) {
    auto *queues = world.get_resource<CommandQueues>();
    if (!queues) {
      throw std::runtime_error("CommandQueues resource not found");
    }

    return Cmd(queues, &world.entities);
  }

  static bool available(const World &world) {
    return world.has_resource<CommandQueues>();
  }

  static void access(SystemAccess &acc) {
    acc.add_resource_read<CommandQueues>();
  }
};

} // namespace ecs
