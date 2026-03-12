#pragma once
#include "entity.hpp"
#include "ticks.hpp"
#include <any>
#include <bit>
#include <cassert>
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
  virtual size_t size() const = 0;
  virtual bool empty() const = 0;
  virtual std::optional<ComponentTicks> get_ticks(Entity entity) const = 0;
};

template <typename T> class SparseSetStorage : public IStorage {
  struct Entry {
    ComponentData<T> data;
    Entity entity;
  };

  std::vector<uint32_t> sparse_;
  std::vector<Entry> dense_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    uint32_t idx = entity.index();
    if (idx >= sparse_.size())
      sparse_.resize(idx + 1, UINT32_MAX);

    if (sparse_[idx] != UINT32_MAX) {
      auto &entry = dense_[sparse_[idx]];
      entry.data.component = std::move(component);
      entry.data.ticks.changed = current_tick;
      return;
    }

    sparse_[idx] = static_cast<uint32_t>(dense_.size());
    dense_.push_back(
        {{std::move(component), {current_tick, current_tick}}, entity});
  }

  T *get(Entity entity) {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX) {
      return &dense_[sparse_[idx]].data.component;
    }
    return nullptr;
  }

  const T *get(Entity entity) const {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX) {
      return &dense_[sparse_[idx]].data.component;
    }
    return nullptr;
  }

  ComponentData<T> *get_data(Entity entity) {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX) {
      return &dense_[sparse_[idx]].data;
    }
    return nullptr;
  }

  const ComponentData<T> *get_data(Entity entity) const {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX) {
      return &dense_[sparse_[idx]].data;
    }
    return nullptr;
  }

  void remove(Entity entity) override {
    uint32_t idx = entity.index();
    if (idx >= sparse_.size() || sparse_[idx] == UINT32_MAX)
      return;

    uint32_t dense_idx = sparse_[idx];
    if (dense_idx != dense_.size() - 1) {
      dense_[dense_idx] = std::move(dense_.back());
      sparse_[dense_[dense_idx].entity.index()] = dense_idx;
    }

    dense_.pop_back();
    sparse_[idx] = UINT32_MAX;
  }

  bool has(Entity entity) const override {
    uint32_t idx = entity.index();
    return idx < sparse_.size() && sparse_[idx] != UINT32_MAX;
  }

  size_t size() const override { return dense_.size(); }
  bool empty() const override { return dense_.empty(); }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto *d = get_data(entity);
    return d ? std::optional(d->ticks) : std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto *d = get_data(entity);
    if (d)
      d->ticks.changed = tick;
  }

  template <typename F> void for_each(F &&func) const {
    for (const auto &entry : dense_) {
      func(entry.entity, entry.data.component);
    }
  }

  template <typename F> void for_each_mut(F &&func) {
    for (auto &entry : dense_) {
      func(entry.entity, entry.data.component);
    }
  }

  size_t data_size() const { return sparse_.size(); }
};

template <typename T> class HashMapStorage : public IStorage {
  std::unordered_map<Entity, ComponentData<T>> data_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    ComponentTicks ticks{current_tick, current_tick};
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

  void remove(Entity entity) override { data_.erase(entity); }
  bool has(Entity entity) const override { return data_.count(entity) > 0; }

  template <typename F> void for_each(F &&func) const {
    for (auto &[e, cd] : data_)
      func(e, cd.component);
  }

  template <typename F> void for_each_mut(F &&func) {
    for (auto &[e, cd] : data_)
      func(e, cd.component);
  }

  size_t size() const override { return data_.size(); }
  bool empty() const override { return data_.empty(); }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto *d = get_data(entity);
    return d ? std::optional(d->ticks) : std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto *d = get_data(entity);
    if (d)
      d->ticks.changed = tick;
  }
};

template <typename T, typename = void> struct storage_trait {
  using type = SparseSetStorage<T>;
};

template <typename T>
struct storage_trait<T, std::void_t<typename T::Storage>> {
  using type = typename T::Storage;
};

template <typename T> using storage_t = typename storage_trait<T>::type;

} // namespace ecs
