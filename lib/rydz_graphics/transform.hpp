#pragma once
#include "math.hpp"
#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/query.hpp"
#include <functional>
#include <unordered_map>

namespace ecs {

using namespace math;

using Transform = math::Transform;
using GlobalTransform = math::GlobalTransform;

inline auto propagate_transforms(
    Query<Entity, Transform, Opt<Parent>, Mut<GlobalTransform>> query) -> void {

  std::unordered_map<Entity, Mat4> local_matrices;
  std::unordered_map<Entity, Entity> parents;

  for (auto [entity, transform, parent, global] : query.iter()) {
    local_matrices[entity] = transform->compute_matrix();
    if (parent != nullptr) {
      parents[entity] = parent->entity;
    }
  }

  std::unordered_map<Entity, GlobalTransform> cache;
  std::function<GlobalTransform(Entity)> compute =
      [&](Entity entity) -> GlobalTransform {
    auto it = cache.find(entity);
    if (it != cache.end()) {
      return it->second;
    }

    Mat4 local_mat = Mat4::IDENTITY;
    auto lit = local_matrices.find(entity);
    if (lit != local_matrices.end()) {
      local_mat = lit->second;
    }

    GlobalTransform result;
    auto pit = parents.find(entity);
    if (pit != parents.end()) {
      GlobalTransform parent_global = compute(pit->second);
      result.matrix = parent_global.matrix * local_mat;
    } else {
      result.matrix = local_mat;
    }

    cache[entity] = result;
    return result;
  };

  for (auto &[entity, _] : local_matrices) {
    compute(entity);
  }

  for (auto [entity, transform, parent, global] : query.iter()) {
    auto it = cache.find(entity);
    if (it != cache.end()) {
      global.bypass_change_detection() = it->second;
    }
  }
}

} // namespace ecs
