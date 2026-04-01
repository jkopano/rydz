#pragma once
#include "entity.hpp"
#include "fwd.hpp"
#include "helpers.hpp"
#include "requires.hpp"
#include "resource.hpp"
#include "rydz_ecs/bundle.hpp"
#include "storage.hpp"
#include "ticks.hpp"
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
  bool multithreaded_ = true;

  template <typename R> void insert_if_missing(Entity entity) {
    using Decayed = std::decay_t<R>;
    if (!this->has_component<Decayed>(entity)) {
      this->insert_component<Decayed>(entity, Decayed{});
    }
  }

public:
  World() = default;

  bool multithreaded() const { return multithreaded_; }
  void set_multithreaded(bool v) { multithreaded_ = v; }

  Tick read_change_tick() const { return change_tick_; }
  Tick increment_change_tick() {
    change_tick_.value++;
    return change_tick_;
  }

  // <RESOURCY>

  template <typename T> void insert_resource(T resource) {
    static_assert(IsResource<T>,
                  "Only Resources can be inserted via insert_resource(). "
                  "Add 'using T = Resource;' to your struct.");
    resources.insert<T>(std::move(resource));
  }

  template <typename T> T *get_resource() { return resources.get<T>(); }

  template <typename T> const T *get_resource() const {
    return resources.get<T>();
  }

  template <typename T> bool has_resource() const { return resources.has<T>(); }

  template <typename T> std::optional<T> remove_resource() {
    return resources.remove<T>();
  }

  // </RESOURCY>

  Entity spawn() { return entities.create(); }

  void despawn(Entity entity) {
    for (auto &[_, storage] : storages_) {
      storage->remove(entity);
    }
    entities.destroy(entity);
  }

  template <typename T> auto &ensure_storage_exist() {
    using TargetStorage = storage_t<T>;

    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it != storages_.end()) {
      return *static_cast<TargetStorage *>(it->second.get());
    }

    auto storage = std::make_unique<TargetStorage>();
    auto *ptr = storage.get();
    storages_.emplace(key, std::move(storage));
    return *ptr;
  }

  template <typename T> void insert_component(Entity entity, T component) {
    assert(entities.is_alive(entity) && "inserting component on dead entity");
    auto &storage = ensure_storage_exist<T>();
    storage.insert(entity, std::move(component), change_tick_);

    if constexpr (ecs::HasRequired<T>) {
      using ReqTuple = typename T::Required::as_tuple;
      std::apply(
          [this, entity](auto... args) {
            (this->insert_if_missing<decltype(args)>(entity), ...);
          },
          ReqTuple{});
    }
  }

  template <typename T, typename Self>
  auto *get_component(this Self &&self, Entity entity) {
    auto *storage = self.template get_storage<T>();
    return (storage == nullptr) ? nullptr : storage->get(entity);
  }

  template <typename T> void remove_component(Entity entity) {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end()) {
      return;
    }

    it->second->remove(entity);
  }

  template <typename T> bool has_component(Entity entity) const {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end())
      return false;
    return it->second->has(entity);
  }

  template <typename T>
  std::optional<ComponentTicks> get_component_ticks(Entity entity) const {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end()) {
      return std::nullopt;
    }
    return it->second->get_ticks(entity);
  }

  template <typename T, typename Self> auto *get_storage(this Self &&self) {
    using StripT = std::remove_cv_t<T>;
    using ReturnT = copy_const_t<Self, storage_t<StripT>>;

    auto it = self.storages_.find(std::type_index(typeid(StripT)));
    return (it == self.storages_.end())
               ? nullptr
               : static_cast<ReturnT *>(it->second.get());
  }
};

// ── deferred definitions from bundle.hpp ──────────────────────────────
// These need the full World definition, so they live here.

namespace detail {

template <typename T>
void insert_single(World &world, Entity entity, T &&item) {
  using Raw = bare_t<T>;
  if constexpr (IsBundle<Raw>) {
    insert_tuple_items(world, entity, to_tuple(std::forward<T>(item)));
  } else if constexpr (IsTuple<Raw>::value) {
    insert_tuple_items(world, entity, std::forward<T>(item));
  } else {
    static_assert(IsComponent<Raw>,
                  "Only Components and Bundles can be inserted on entities. "
                  "Add 'using T = Component;' or 'using T = Bundle;'.");
    world.insert_component(entity, std::forward<T>(item));
  }
}

} // namespace detail

template <std::ranges::input_range R>
  requires Spawnable<std::ranges::range_value_t<R>>
std::vector<Entity> spawn_batch(World &world, R &&_range) {
  std::vector<Entity> spawned;
  if constexpr (std::ranges::sized_range<R>)
    spawned.reserve(std::ranges::size(_range));

  for (auto &&item : std::forward<R>(_range)) {
    Entity e = world.spawn();
    insert_bundle(world, e, std::forward<decltype(item)>(item));
    spawned.push_back(e);
  }
  return spawned;
}

} // namespace ecs
