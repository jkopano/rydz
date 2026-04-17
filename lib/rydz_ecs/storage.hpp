#pragma once
#include "entity.hpp"
#include "ticks.hpp"
#include <cassert>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace ecs {

class IStorage {
public:
  virtual ~IStorage() = default;
  virtual auto remove(Entity entity) -> void = 0;
  [[nodiscard]] virtual auto has(Entity entity) const -> bool = 0;
  [[nodiscard]] virtual auto size() const -> usize = 0;
  [[nodiscard]] virtual auto empty() const -> bool = 0;
  [[nodiscard]] virtual auto get_ticks(Entity entity) const
      -> std::optional<ComponentTicks> = 0;
  [[nodiscard]] virtual auto entities() const -> std::span<const Entity> = 0;
};

template <typename T> class SparseSetStorage : public IStorage {
  std::vector<u32> sparse_;
  std::vector<T> dense_data_;
  std::vector<ComponentTicks> dense_ticks_;
  std::vector<Entity> dense_entities_;

  [[nodiscard]] auto get_dense_idx(Entity e) const -> std::optional<u32> {
    u32 idx = e.index();
    if (idx < sparse_.size() && sparse_[idx] != UINT32_MAX &&
        dense_entities_[sparse_[idx]] == e) {
      return sparse_[idx];
    }
    return std::nullopt;
  }

public:
  auto insert(Entity entity, T component, Tick current_tick) -> void {
    if (auto d_idx = get_dense_idx(entity)) {
      dense_data_[*d_idx] = std::move(component);
      dense_ticks_[*d_idx].changed = current_tick;
      return;
    }

    u32 idx = entity.index();
    if (idx >= sparse_.size()) {
      sparse_.resize(idx + 1, UINT32_MAX);
    }

    sparse_[idx] = static_cast<u32>(dense_data_.size());
    dense_data_.push_back(std::move(component));
    dense_ticks_.push_back({current_tick, current_tick});
    dense_entities_.push_back(entity);
  }

  template <typename Self> auto get(this Self &self, Entity entity) -> auto * {
    auto idx = self.get_dense_idx(entity);
    return idx ? &self.dense_data_[*idx] : nullptr;
  }

  auto get_ticks_mut(Entity enitity) -> ComponentTicks * {
    auto idx = get_dense_idx(enitity);
    return idx ? &dense_ticks_[*idx] : nullptr;
  }

  auto get_with_ticks(Entity entity) -> std::pair<T *, ComponentTicks *> {
    auto idx = get_dense_idx(entity);
    if (idx) {
      return {&dense_data_[*idx], &dense_ticks_[*idx]};
    }
    return {nullptr, nullptr};
  }

  auto remove(Entity entity) -> void override {
    auto d_idx = get_dense_idx(entity);
    if (!d_idx) {
      return;
    }

    u32 last = static_cast<u32>(dense_data_.size() - 1);
    if (*d_idx != last) {
      dense_data_[*d_idx] = std::move(dense_data_[last]);
      dense_ticks_[*d_idx] = dense_ticks_[last];
      dense_entities_[*d_idx] = dense_entities_[last];
      sparse_[dense_entities_[*d_idx].index()] = *d_idx;
    }

    dense_data_.pop_back();
    dense_ticks_.pop_back();
    dense_entities_.pop_back();
    sparse_[entity.index()] = UINT32_MAX;
  }

  auto has(Entity e) const -> bool override {
    return get_dense_idx(e).has_value();
  }
  auto size() const -> usize override { return dense_data_.size(); }
  auto empty() const -> bool override { return dense_data_.empty(); }
  auto entities() const -> std::span<const Entity> override {
    return dense_entities_;
  }

  auto get_ticks(Entity e) const -> std::optional<ComponentTicks> override {
    auto i = get_dense_idx(e);
    return i ? std::optional(dense_ticks_[*i]) : std::nullopt;
  }

  auto mark_changed(Entity entity, Tick tick) -> void {
    if (auto i = get_dense_idx(entity))
      dense_ticks_[*i].changed = tick;
  }

  template <typename F> void for_each(F &&func) const {
    for (usize i = 0; i < dense_data_.size(); ++i)
      func(dense_entities_[i], dense_data_[i]);
  }

  template <typename F> void for_each_mut(F &&func) {
    for (usize i = 0; i < dense_data_.size(); ++i)
      func(dense_entities_[i], dense_data_[i]);
  }
};

template <typename T> class HashMapStorage : public IStorage {
  std::unordered_map<Entity, T> data_;
  std::unordered_map<Entity, ComponentTicks> ticks_;
  std::vector<Entity> entity_keys_;

public:
  auto insert(Entity entity, T component, Tick current_tick) -> void {
    if (data_.find(entity) == data_.end())
      entity_keys_.push_back(entity);
    data_.insert_or_assign(entity, std::move(component));
    ticks_.insert_or_assign(entity, ComponentTicks{current_tick, current_tick});
  }

  auto get(Entity entity) -> T * {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  auto get(Entity entity) const -> const T * {
    auto it = data_.find(entity);
    return it != data_.end() ? &it->second : nullptr;
  }

  auto get_ticks_mut(Entity entity) -> ComponentTicks * {
    auto it = ticks_.find(entity);
    return it != ticks_.end() ? &it->second : nullptr;
  }

  auto get_ticks_ptr(Entity entity) const -> const ComponentTicks * {
    auto it = ticks_.find(entity);
    return it != ticks_.end() ? &it->second : nullptr;
  }

  auto get_with_ticks(Entity entity) -> std::pair<T *, ComponentTicks *> {
    auto it = data_.find(entity);
    if (it != data_.end()) {
      auto ticks_it = ticks_.find(entity);
      return {&it->second, &ticks_it->second};
    }
    return {nullptr, nullptr};
  }

  auto remove(Entity entity) -> void override {
    data_.erase(entity);
    ticks_.erase(entity);
    std::erase(entity_keys_, entity);
  }

  auto has(Entity entity) const -> bool override {
    return data_.contains(entity);
  }

  template <typename F> void for_each(F &&func) const {
    for (auto &[e, comp] : data_)
      func(e, comp);
  }

  template <typename F> void for_each_mut(F &&func) {
    for (auto &[e, comp] : data_)
      func(e, comp);
  }

  auto size() const -> usize override { return data_.size(); }
  auto empty() const -> bool override { return data_.empty(); }
  auto data_size() const -> usize { return data_.size(); }

  auto entities() const -> std::span<const Entity> override {
    return entity_keys_;
  }

  auto get_ticks(Entity entity) const
      -> std::optional<ComponentTicks> override {
    auto *t = get_ticks_ptr(entity);
    return t ? std::optional(*t) : std::nullopt;
  }

  auto mark_changed(Entity entity, Tick tick) -> void {
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
