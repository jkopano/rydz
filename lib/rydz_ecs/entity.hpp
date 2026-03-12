#pragma once
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <vector>

namespace ecs {

struct Entity {
  uint32_t index_val = 0;
  uint32_t generation_val = 0;

  static constexpr Entity from_raw(uint32_t idx, uint32_t gen) {
    return {idx, gen};
  }

  [[nodiscard]] constexpr uint32_t index() const noexcept { return index_val; }
  [[nodiscard]] constexpr uint32_t generation() const noexcept { return generation_val; }

  auto operator<=>(const Entity &) const = default;
};

} // namespace ecs

template <> struct std::hash<ecs::Entity> {
  size_t operator()(const ecs::Entity &e) const noexcept {
    return std::hash<uint64_t>{}((static_cast<uint64_t>(e.index_val) << 32) |
                                 e.generation_val);
  }
};

namespace ecs {

class EntityManager {
  uint32_t next_id_ = 0;
  std::vector<std::pair<uint32_t, uint32_t>> free_list_;
  std::unordered_set<Entity> active_;

public:
  Entity create() {
    Entity e;
    if (!free_list_.empty()) {
      auto [idx, gen] = free_list_.back();
      free_list_.pop_back();
      e = Entity::from_raw(idx, gen);
    } else {
      e = Entity::from_raw(next_id_++, 0);
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

  bool is_alive(Entity entity) const { return active_.count(entity) > 0; }

  size_t count() const { return active_.size(); }
};

} // namespace ecs
