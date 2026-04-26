#pragma once
#include "entity.hpp"
#include "fwd.hpp"
#include "helpers.hpp"
#include "requires.hpp"
#include "resource.hpp"
#include "rydz_ecs/bundle.hpp"
#include "storage.hpp"
#include "ticks.hpp"
#include <atomic>
#include <cassert>
#include <memory>
#include <ranges>
#include <typeindex>
#include <unordered_map>

namespace ecs {

class World {
public:
  EntityManager entities;
  Resources resources;

private:
  std::unordered_map<std::type_index, std::unique_ptr<IStorage>> storages_;
  Tick change_tick_{1};
  std::atomic<u64> schedule_run_id_{0};
  bool multithreaded_ = true;

  template <typename R> void insert_if_missing(Entity entity) {
    using Decayed = std::decay_t<R>;
    if (!this->has_component<Decayed>(entity)) {
      this->insert_component<Decayed>(entity, Decayed{});
    }
  }

  auto despawn_immediate(Entity entity) -> void;
  auto ensure_observer_registry() -> ObserverRegistry&;
  auto observer_registry() const -> ObserverRegistry const*;

public:
  World() = default;

  auto multithreaded() const -> bool { return multithreaded_; }
  auto set_multithreaded(bool v) -> void { multithreaded_ = v; }

  auto read_change_tick() const -> Tick { return change_tick_; }
  auto read_schedule_run_id() const -> u64 {
    return schedule_run_id_.load(std::memory_order_relaxed);
  }
  auto begin_schedule_run() -> u64 {
    return schedule_run_id_.fetch_add(1, std::memory_order_relaxed) + 1;
  }
  auto increment_change_tick() -> Tick {
    change_tick_.value++;
    return change_tick_;
  }

  // <RESOURCY>

  template <typename T> void insert_resource(T resource) {
    static_assert(
      IsResource<T>,
      "Only Resources can be inserted via insert_resource(). "
      "Add 'using T = Resource;' to your struct."
    );
    resources.insert<T>(std::move(resource));
  }

  template <typename T> auto get_resource() -> T* { return resources.get<T>(); }

  template <typename T> auto get_resource() const -> T const* {
    return resources.get<T>();
  }

  template <typename T> auto has_resource() const -> bool {
    return resources.has<T>();
  }

  template <typename T> auto remove_resource() -> std::optional<T> {
    return resources.remove<T>();
  }

  // </RESOURCY>

  auto spawn() -> Entity { return entities.create(); }

  auto despawn(Entity entity) -> void;

  template <typename E>
    requires IsEvent<bare_t<E>>
  auto add_event() -> void;

  template <typename F> auto add_observer(F&& func) -> Entity;

  template <typename F> auto add_observer(Entity target, F&& func) -> Entity;

  template <typename E>
    requires IsEvent<bare_t<E>>
  auto trigger(E&& event) -> void;

  template <typename T> auto ensure_storage_exist() -> auto& {
    using TargetStorage = storage_t<T>;

    auto key = std::type_index(typeid(T));
    auto iter = storages_.find(key);
    if (iter != storages_.end()) {
      return *static_cast<TargetStorage*>(iter->second.get());
    }

    auto storage = std::make_unique<TargetStorage>();
    auto* ptr = storage.get();
    storages_.emplace(key, std::move(storage));
    return *ptr;
  }

  template <typename T> void insert_component(Entity entity, T component) {
    assert(entities.is_alive(entity) && "inserting component on dead entity");
    auto& storage = ensure_storage_exist<T>();
    storage.insert(entity, std::move(component), change_tick_);

    if constexpr (ecs::HasRequired<T>) {
      using ReqTuple = typename T::Required::as_tuple;
      std::apply(
        [this, entity](auto... args) -> auto {
          (this->insert_if_missing<decltype(args)>(entity), ...);
        },
        ReqTuple{}
      );
    }
  }

  template <typename T, typename Self>
  auto get_component(this Self& self, Entity entity) -> auto* {
    auto* storage = self.template get_storage<T>();
    return (storage == nullptr) ? nullptr : storage->get(entity);
  }

  template <typename T> void remove_component(Entity entity) {
    auto key = std::type_index(typeid(T));
    auto iter = storages_.find(key);
    if (iter == storages_.end()) {
      return;
    }

    iter->second->remove(entity);
  }

  template <typename T> auto has_component(Entity entity) const -> bool {
    auto key = std::type_index(typeid(T));
    auto iter = storages_.find(key);
    if (iter == storages_.end()) {
      return false;
    }
    return iter->second->has(entity);
  }

  template <typename T>
  auto get_component_ticks(Entity entity) const
    -> std::optional<ComponentTicks> {
    auto key = std::type_index(typeid(T));
    auto iter = storages_.find(key);
    if (iter == storages_.end()) {
      return std::nullopt;
    }
    return iter->second->get_ticks(entity);
  }

  template <typename T, typename Self>
  auto get_storage(this Self& self) -> auto* {
    using StripT = std::remove_cv_t<T>;
    using ReturnT = copy_const_t<Self, storage_t<StripT>>;

    auto iter = self.storages_.find(std::type_index(typeid(StripT)));
    return (iter == self.storages_.end())
             ? nullptr
             : static_cast<ReturnT*>(iter->second.get());
  }
};

namespace detail {

template <typename T>
auto insert_single(World& world, Entity entity, T&& item) -> void {
  using Raw = bare_t<T>;
  if constexpr (IsBundle<Raw>) {
    insert_tuple_items(world, entity, to_tuple(std::forward<T>(item)));
  } else if constexpr (IsTuple<Raw>::value) {
    insert_tuple_items(world, entity, std::forward<T>(item));
  } else {
    static_assert(
      IsComponent<Raw>,
      "Only Components and Bundles can be inserted on entities. "
      "Add 'using T = Component;' or 'using T = Bundle;'."
    );
    world.insert_component(entity, std::forward<T>(item));
  }
}

} // namespace detail

template <std::ranges::input_range R>
  requires Spawnable<std::ranges::range_value_t<R>>
auto spawn_batch(World& world, R&& _range) -> std::vector<Entity> {
  std::vector<Entity> spawned;
  if constexpr (std::ranges::sized_range<R>) {
    spawned.reserve(std::ranges::size(_range));
  }

  for (auto&& item : std::forward<R>(_range)) {
    Entity entity = world.spawn();
    insert_bundle(world, entity, std::forward<decltype(item)>(item));
    spawned.push_back(entity);
  }
  return spawned;
}

} // namespace ecs

#include "event.hpp"
