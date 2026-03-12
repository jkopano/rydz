#pragma once
#include "entity.hpp"
#include "hierarchy.hpp"
#include "world.hpp"
#include <functional>
#include <unordered_map>

namespace ecs {

enum class Visibility {
  Inherited,
  Visible,
  Hidden,
};

struct ComputedVisibility {
  bool visible = true;
};

inline void compute_visibility(World &world) {
  std::unordered_map<Entity, Visibility> visibilities;
  auto *vis_storage = world.get_storage<Visibility>();
  if (!vis_storage)
    return;

  auto entities = vis_storage->entities();
  for (auto e : entities) {
    auto *v = vis_storage->get(e);
    if (v)
      visibilities[e] = *v;
  }

  std::unordered_map<Entity, Entity> parents;
  auto *parent_storage = world.get_storage<Parent>();
  if (parent_storage) {
    auto p_entities = parent_storage->entities();
    for (auto e : p_entities) {
      auto *p = parent_storage->get(e);
      if (p)
        parents[e] = p->entity;
    }
  }

  std::function<bool(Entity)> is_visible = [&](Entity entity) -> bool {
    auto vit = visibilities.find(entity);
    Visibility self_vis =
        (vit != visibilities.end()) ? vit->second : Visibility::Inherited;

    if (self_vis == Visibility::Hidden)
      return false;
    if (self_vis == Visibility::Visible)
      return true;

    auto pit = parents.find(entity);
    if (pit != parents.end()) {
      return is_visible(pit->second);
    }
    return true;
  };

  for (auto e : entities) {
    world.insert_component(e, ComputedVisibility{is_visible(e)});
  }
}

} // namespace ecs
