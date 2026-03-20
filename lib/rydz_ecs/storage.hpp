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
  virtual std::vector<Entity> entities() const = 0;
};

template <typename T> class SparseSetStorage : public IStorage {
  std::vector<uint32_t> sparse_;
  std::vector<T> dense_data_;
  std::vector<ComponentTicks> dense_ticks_;
  std::vector<Entity> dense_entities_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    uint32_t idx = entity.index();
    if (idx >= sparse_.size())
      sparse_.resize(idx + 1, UINT32_MAX);

    if (sparse_[idx] != UINT32_MAX) {
      uint32_t dense_idx = sparse_[idx];
      if (dense_entities_[dense_idx] == entity) {
        dense_data_[dense_idx] = std::move(component);
        dense_ticks_[dense_idx].changed = current_tick;
        return;
      }

      dense_data_[dense_idx] = std::move(component);
      dense_ticks_[dense_idx] = ComponentTicks{current_tick, current_tick};
      dense_entities_[dense_idx] = entity;
      return;
    }

    sparse_[idx] = static_cast<uint32_t>(dense_data_.size());
    dense_data_.push_back(std::move(component));
    dense_ticks_.push_back(ComponentTicks{current_tick, current_tick});
    dense_entities_.push_back(entity);
  }

  T *get(Entity entity) {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
        dense_entities_[sparse_[idx]] == entity) {
      return &dense_data_[sparse_[idx]];
    }
    return nullptr;
  }

  const T *get(Entity entity) const {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
        dense_entities_[sparse_[idx]] == entity) {
      return &dense_data_[sparse_[idx]];
    }
    return nullptr;
  }

  ComponentTicks *get_ticks_mut(Entity entity) {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
        dense_entities_[sparse_[idx]] == entity) {
      return &dense_ticks_[sparse_[idx]];
    }
    return nullptr;
  }

  const ComponentTicks *get_ticks_ptr(Entity entity) const {
    uint32_t idx = entity.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
        dense_entities_[sparse_[idx]] == entity) {
      return &dense_ticks_[sparse_[idx]];
    }
    return nullptr;
  }

  void remove(Entity entity) override {
    uint32_t idx = entity.index();
    if (idx >= sparse_.size() || sparse_[idx] == UINT32_MAX ||
        dense_entities_[sparse_[idx]] != entity)
      return;

    uint32_t dense_idx = sparse_[idx];
    uint32_t last = static_cast<uint32_t>(dense_data_.size() - 1);
    if (dense_idx != last) {
      dense_data_[dense_idx] = std::move(dense_data_[last]);
      dense_ticks_[dense_idx] = dense_ticks_[last];
      dense_entities_[dense_idx] = dense_entities_[last];
      sparse_[dense_entities_[dense_idx].index()] = dense_idx;
    }

    dense_data_.pop_back();
    dense_ticks_.pop_back();
    dense_entities_.pop_back();
    sparse_[idx] = UINT32_MAX;
  }

  bool has(Entity entity) const override {
    uint32_t idx = entity.index();
    return idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
           dense_entities_[sparse_[idx]] == entity;
  }

  size_t size() const override { return dense_data_.size(); }
  bool empty() const override { return dense_data_.empty(); }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto *t = get_ticks_ptr(entity);
    return t ? std::optional(*t) : std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto *t = get_ticks_mut(entity);
    if (t)
      t->changed = tick;
  }

  template <typename F> void for_each(F &&func) const {
    for (size_t i = 0; i < dense_data_.size(); ++i) {
      func(dense_entities_[i], dense_data_[i]);
    }
  }

  template <typename F> void for_each_mut(F &&func) {
    for (size_t i = 0; i < dense_data_.size(); ++i) {
      func(dense_entities_[i], dense_data_[i]);
    }
  }

  size_t data_size() const { return sparse_.size(); }

  std::vector<Entity> entities() const override {
    return dense_entities_;
  }
};

template <typename T> class HashMapStorage : public IStorage {
  std::unordered_map<Entity, T> data_;
  std::unordered_map<Entity, ComponentTicks> ticks_;

public:
  void insert(Entity entity, T component, Tick current_tick) {
    data_.insert_or_assign(entity, std::move(component));
    ticks_.insert_or_assign(entity,
                            ComponentTicks{current_tick, current_tick});
  }

  T *get(Entity entity) {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  const T *get(Entity entity) const {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  ComponentTicks *get_ticks_mut(Entity entity) {
    auto it = ticks_.find(entity);
    return it != ticks_.end() ? &it->second : nullptr;
  }

  const ComponentTicks *get_ticks_ptr(Entity entity) const {
    auto it = ticks_.find(entity);
    return it != ticks_.end() ? &it->second : nullptr;
  }

  void remove(Entity entity) override {
    data_.erase(entity);
    ticks_.erase(entity);
  }

  bool has(Entity entity) const override { return data_.count(entity) > 0; }

  template <typename F> void for_each(F &&func) const {
    for (auto &[e, comp] : data_)
      func(e, comp);
  }

  template <typename F> void for_each_mut(F &&func) {
    for (auto &[e, comp] : data_)
      func(e, comp);
  }

  size_t size() const override { return data_.size(); }
  bool empty() const override { return data_.empty(); }
  size_t data_size() const { return data_.size(); }

  std::vector<Entity> entities() const override {
    std::vector<Entity> result;
    result.reserve(data_.size());
    for (const auto &[e, _] : data_)
      result.push_back(e);
    return result;
  }

  std::optional<ComponentTicks> get_ticks(Entity entity) const override {
    auto *t = get_ticks_ptr(entity);
    return t ? std::optional(*t) : std::nullopt;
  }

  void mark_changed(Entity entity, Tick tick) {
    auto *t = get_ticks_mut(entity);
    if (t)
      t->changed = tick;
  }
};

template <typename T, typename = void> struct storage_trait {
  using type = SparseSetStorage<std::remove_cv_t<T>>;
};

template <typename T>
struct storage_trait<T, std::void_t<typename std::remove_cv_t<T>::Storage>> {
  using type = typename std::remove_cv_t<T>::Storage;
};

template <typename T> using storage_t = typename storage_trait<T>::type;

} // namespace ecs
