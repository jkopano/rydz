#pragma once
#include "entity.hpp"
#include "world.hpp"
#include <vector>

namespace ecs {

struct Parent {
  Entity entity;
};

struct Children {
  std::vector<Entity> entities;

  size_t size() const { return entities.size(); }
  bool empty() const { return entities.empty(); }

  auto begin() const { return entities.begin(); }
  auto end() const { return entities.end(); }
};

inline Children children_of(const World &world, Entity parent) {
  Children result;

  auto *storage = world.get_storage<Parent>();
  if (!storage)
    return result;

  auto indices = storage->entities();
  for (auto entity : indices) {
    auto *p = storage->get(entity);
    if (p && p->entity == parent) {
      result.entities.push_back(entity);
    }
  }
  return result;
}

} // namespace ecs
