#pragma once
#include "entity.hpp"
#include "ticks.hpp"
#include <any>
#include <functional>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ecs {

class IStorage {
public:
  virtual ~IStorage() = default;
  virtual void remove(Entity entity) = 0;
  virtual bool has(Entity entity) const = 0;
  virtual std::vector<Entity> entity_indices() const = 0;
  virtual size_t size() const = 0;
  virtual bool empty() const = 0;
  virtual std::optional<ComponentTicks> get_ticks(Entity entity) const = 0;
};

template <typename T> class VecStorage : public IStorage {
  std::vector<std::optional<ComponentData<T>>> data_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    uint32_t idx = entity.index;
    if (idx >= data_.size()) {
      data_.resize(std::bit_ceil(idx + 1), std::nullopt);
    }
    ComponentTicks ticks;
    ticks.added = current_tick;
    ticks.changed = current_tick;
    data_[idx] = ComponentData<T>{std::move(component), ticks};
  }

  T *get(Entity entity) {
    uint32_t idx = entity.index;
    if (idx < data_.size() && data_[idx].has_value()) {
      return &data_[idx]->component;
    }
    return nullptr;
  }

  const T *get(Entity entity) const {
    uint32_t idx = entity.index;
    if (idx < data_.size() && data_[idx].has_value()) {
      return &data_[idx]->component;
    }
    return nullptr;
  }

  ComponentData<T> *get_data(Entity entity) {
    uint32_t idx = entity.index;
    if (idx < data_.size() && data_[idx].has_value()) {
      return &data_[idx].value();
    }
    return nullptr;
  }

  const ComponentData<T> *get_data(Entity entity) const {
    uint32_t idx = entity.index;
    if (idx < data_.size() && data_[idx].has_value()) {
      return &data_[idx].value();
    }
    return nullptr;
  }

  template <typename F> void for_each(F &&func) const {
    for (uint32_t i = 0; i < data_.size(); ++i) {
      if (data_[i].has_value()) {
        Entity e = Entity::from_raw(i, 0);
        func(e, data_[i]->component);
      }
    }
  }

  template <typename F> void for_each_mut(F &&func) {
    for (uint32_t i = 0; i < data_.size(); ++i) {
      if (data_[i].has_value()) {
        Entity e = Entity::from_raw(i, 0);
        func(e, data_[i]->component);
      }
    }
  }

  void remove(Entity entity) override {
    uint32_t idx = entity.index;
    if (idx < data_.size()) {
      data_[idx] = std::nullopt;
    }
  }

  bool has(Entity entity) const override {
    uint32_t idx = entity.index;
    return idx < data_.size() && data_[idx].has_value();
  }

  std::vector<Entity> entity_indices() const override {
    std::vector<Entity> result;
    for (uint32_t i = 0; i < data_.size(); ++i) {
      if (data_[i].has_value()) {
        result.push_back(Entity::from_raw(i, 0));
      }
    }
    return result;
  }

  size_t size() const override {
    size_t count = 0;
    for (auto &slot : data_)
      if (slot.has_value())
        ++count;
    return count;
  }

  bool empty() const override { return size() == 0; }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto *d = get_data(entity);
    if (d)
      return d->ticks;
    return std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto *d = get_data(entity);
    if (d)
      d->ticks.changed = tick;
  }

  size_t data_size() const { return data_.size(); }
};

template <typename T> class HashMapStorage : public IStorage {
  std::unordered_map<Entity, ComponentData<T>> data_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    ComponentTicks ticks;
    ticks.added = current_tick;
    ticks.changed = current_tick;
    data_.insert_or_assign(entity,
                           ComponentData<T>{std::move(component), ticks});
  }

  T *get(Entity entity) {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second.component : nullptr;
  }

  const T *get(Entity entity) const {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second.component : nullptr;
  }

  ComponentData<T> *get_data(Entity entity) {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  const ComponentData<T> *get_data(Entity entity) const {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  template <typename F> void for_each(F &&func) const {
    for (auto &[e, cd] : data_) {
      func(e, cd.component);
    }
  }

  template <typename F> void for_each_mut(F &&func) {
    for (auto &[e, cd] : data_) {
      func(e, cd.component);
    }
  }

  void remove(Entity entity) override { data_.erase(entity); }
  bool has(Entity entity) const override { return data_.count(entity) > 0; }

  std::vector<Entity> entity_indices() const override {
    std::vector<Entity> result;
    result.reserve(data_.size());
    for (auto &[e, _] : data_)
      result.push_back(e);
    return result;
  }

  size_t size() const override { return data_.size(); }
  bool empty() const override { return data_.empty(); }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto it = data_.find(entity);
    if (it != data_.end())
      return it->second.ticks;
    return std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto it = data_.find(entity);
    if (it != data_.end())
      it->second.ticks.changed = tick;
  }
};

template <typename T, typename = void> struct storage_trait {
  using type = VecStorage<T>;
};

template <typename T>
struct storage_trait<T, std::void_t<typename T::Storage>> {
  using type = typename T::Storage;
};

template <typename T> using storage_t = typename storage_trait<T>::type;

} // namespace ecs
