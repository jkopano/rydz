#pragma once
#include "types.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace ecs {

struct Entity {
  u32 index_val = 0;
  u32 generation_val = 0;

  static constexpr Entity from_raw(u32 idx, u32 gen) { return {idx, gen}; }

  [[nodiscard]] constexpr u32 index() const noexcept { return index_val; }
  [[nodiscard]] constexpr u32 generation() const noexcept {
    return generation_val;
  }

  auto operator<=>(const Entity &) const = default;
};

} // namespace ecs

template <> struct std::hash<ecs::Entity> {
  size_t operator()(const ecs::Entity &e) const noexcept {
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
  std::unordered_set<Entity> active_;

public:
  Entity create() {
    std::lock_guard lock(*mutex_);

    Entity e;
    if (free_list_.empty()) {
      e = Entity::from_raw(next_id_++, 0);
    } else {
      auto [idx, gen] = free_list_.back();
      free_list_.pop_back();
      e = Entity::from_raw(idx, gen);
    }

    active_.insert(e);
    return e;
  }

  void destroy(Entity entity) {
    if (active_.erase(entity)) {
      free_list_.push_back({entity.index(), entity.generation() + 1});
    }
  }

  std::vector<Entity> entities() const {
    return {active_.begin(), active_.end()};
  }

  bool is_alive(Entity entity) const { return active_.contains(entity); }

  size_t count() const { return active_.size(); }
};

} // namespace ecs
