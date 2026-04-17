#pragma once
#include "types.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace ecs {

struct Entity {
  u32 index_val{};
  u32 generation_val{};

  static constexpr auto from_raw(u32 idx, u32 gen) -> Entity {
    return {.index_val = idx, .generation_val = gen};
  }

  [[nodiscard]] constexpr auto index() const noexcept -> u32 {
    return index_val;
  }
  [[nodiscard]] constexpr auto generation() const noexcept -> u32 {
    return generation_val;
  }

  auto operator<=>(const Entity &) const = default;
};

} // namespace ecs

template <> struct std::hash<ecs::Entity> {
  auto operator()(const ecs::Entity &e) const noexcept -> size_t {
    return std::hash<u64>{}((static_cast<u64>(e.index_val) << 32) |
                            e.generation_val);
  }
};

namespace ecs {

struct EntityManager {
private:
  std::unique_ptr<std::mutex> mutex_ = std::make_unique<std::mutex>();
  u32 next_id_ = 0;
  std::vector<std::pair<u32, u32>> free_list_;
  std::unordered_set<Entity> reserved_;
  std::unordered_set<Entity> active_;

public:
  auto reserve() -> Entity {
    std::lock_guard lock(*mutex_);

    Entity entity;
    if (free_list_.empty()) {
      entity = Entity::from_raw(next_id_++, 0);
    } else {
      auto [idx, gen] = free_list_.back();
      free_list_.pop_back();
      entity = Entity::from_raw(idx, gen);
    }

    reserved_.insert(entity);
    return entity;
  }

  auto create() -> Entity {
    Entity entity = reserve();
    std::lock_guard lock(*mutex_);
    reserved_.erase(entity);
    active_.insert(entity);
    return entity;
  }

  auto activate(Entity entity) -> void {
    std::lock_guard lock(*mutex_);
    if (reserved_.erase(entity) == 0U) {
      return;
    }
    active_.insert(entity);
  }

  auto destroy(Entity entity) -> void {
    std::lock_guard lock(*mutex_);
    if (reserved_.erase(entity) != 0U) {
      free_list_.emplace_back(entity.index(), entity.generation() + 1);
      return;
    }

    if (active_.erase(entity) != 0U) {
      free_list_.emplace_back(entity.index(), entity.generation() + 1);
    }
  }

  auto entities() const -> std::vector<Entity> {
    std::lock_guard lock(*mutex_);
    return {active_.begin(), active_.end()};
  }

  auto is_alive(Entity entity) const -> bool {
    std::lock_guard lock(*mutex_);
    return active_.contains(entity);
  }

  auto count() const -> size_t {
    std::lock_guard lock(*mutex_);
    return active_.size();
  }
};

} // namespace ecs
