#pragma once
#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/world.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
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

inline auto compute_visibility(World& world) -> void {
  auto* vis_storage = world.get_storage<Visibility>();

  std::unordered_map<Entity, Visibility> visibilities;
  std::unordered_set<Entity> entity_set;

  if (vis_storage != nullptr) {
    vis_storage->for_each([&](Entity e, Visibility const& v) -> void {
      visibilities[e] = v;
      entity_set.insert(e);
    });
  }

  std::unordered_map<Entity, Entity> parents;

  auto* parent_storage = world.get_storage<Parent>();
  if (parent_storage != nullptr) {
    parent_storage->for_each([&](Entity e, Parent const& p) -> void {
      parents[e] = p.entity;
      entity_set.insert(e);
      if (world.entities.is_alive(p.entity)) {
        entity_set.insert(p.entity);
      }
    });
  }

  if (auto* computed_storage = world.get_storage<ComputedVisibility>()) {
    for (Entity e : computed_storage->entities()) {
      entity_set.insert(e);
    }
  }

  std::function<bool(Entity)> is_visible = [&](Entity entity) -> bool {
    auto vit = visibilities.find(entity);
    Visibility self_vis =
      (vit != visibilities.end()) ? vit->second : Visibility::Inherited;

    if (self_vis == Visibility::Hidden) {
      return false;
    }
    if (self_vis == Visibility::Visible) {
      return true;
    }

    auto pit = parents.find(entity);
    if (pit != parents.end()) {
      return is_visible(pit->second);
    }
    return true;
  };

  for (auto e : entity_set) {
    if (!world.entities.is_alive(e)) {
      continue;
    }
    world.insert_component(e, ComputedVisibility{is_visible(e)});
  }
}

} // namespace ecs
