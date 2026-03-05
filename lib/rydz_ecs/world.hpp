#pragma once
#include "entity.hpp"
#include "helpers.hpp"
#include "resource.hpp"
#include "storage.hpp"
#include "ticks.hpp"
#include <cassert>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace ecs {

class World {
public:
  EntityManager entities;
  Resources resources;

private:
  std::unordered_map<std::type_index, std::unique_ptr<IStorage>> storages_;
  Tick change_tick_{0};

public:
  World() = default;

  Tick read_change_tick() const { return change_tick_; }
  Tick increment_change_tick() {
    change_tick_.value++;
    return change_tick_;
  }

  // <RESOURCY>

  template <typename T> void insert_resource(T resource) {
    resources.insert<T>(std::move(resource));
  }

  template <typename T> T *get_resource() { return resources.get<T>(); }

  template <typename T> const T *get_resource() const {
    return resources.get<T>();
  }

  template <typename T> bool contains_resource() const {
    return resources.contains<T>();
  }

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

  template <typename T> VecStorage<T> &ensure_vec_storage() {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end()) {
      auto storage = std::make_unique<VecStorage<T>>();
      auto *ptr = storage.get();
      storages_.emplace(key, std::move(storage));
      return *ptr;
    }
    return *static_cast<VecStorage<T> *>(it->second.get());
  }

  template <typename T> HashMapStorage<T> &ensure_hashmap_storage() {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end()) {
      auto storage = std::make_unique<HashMapStorage<T>>();
      auto *ptr = storage.get();
      storages_.emplace(key, std::move(storage));
      return *ptr;
    }
    return *static_cast<HashMapStorage<T> *>(it->second.get());
  }

  template <typename T> void insert_component(Entity entity, T component) {
    auto &storage = ensure_vec_storage<T>();
    storage.insert(entity, std::move(component), change_tick_);
  }

  template <typename T> T *get_component(Entity entity) {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end())
      return nullptr;
    return static_cast<VecStorage<T> *>(it->second.get())->get(entity);
  }

  template <typename T> const T *get_component(Entity entity) const {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it == storages_.end())
      return nullptr;
    return static_cast<const VecStorage<T> *>(it->second.get())->get(entity);
  }

  template <typename T> void remove_component(Entity entity) {
    auto key = std::type_index(typeid(T));
    auto it = storages_.find(key);
    if (it != storages_.end()) {
      it->second->remove(entity);
    }
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
    if (it == storages_.end())
      return std::nullopt;
    return it->second->get_ticks(entity);
  }

  template <typename Self>
  auto *get_storage(this Self &&self, std::type_index key) {
    using ReturnType = copy_const_t<Self, IStorage>;

    auto it = self.storages_.find(key);
    return (it == self.storages_.end())
               ? nullptr
               : static_cast<ReturnType *>(it->second.get());
  }

  template <typename T, typename Self> auto *get_vec_storage(this Self &&self) {
    using ReturnType = copy_const_t<Self, VecStorage<T>>;

    auto it = self.storages_.find(std::type_index(typeid(T)));
    return (it == self.storages_.end())
               ? nullptr
               : static_cast<ReturnType *>(it->second.get());
  }
};

} // namespace ecs
