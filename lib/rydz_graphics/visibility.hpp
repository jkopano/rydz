#pragma once
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/hierarchy.hpp"
#include "rydz_ecs/world.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

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

  std::vector<Entity> entities;
  vis_storage->for_each([&](Entity e, const Visibility &v) {
    visibilities[e] = v;
    entities.push_back(e);
  });

  std::unordered_map<Entity, Entity> parents;
  auto *parent_storage = world.get_storage<Parent>();
  if (parent_storage) {
    parent_storage->for_each(
        [&](Entity e, const Parent &p) { parents[e] = p.entity; });
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
