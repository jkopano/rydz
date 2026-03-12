#pragma once
#include "rl.hpp"
#include "rydz_ecs/hierarchy.hpp"
#include "rydz_ecs/query.hpp"
#include <functional>
#include <unordered_map>

namespace ecs {

struct Transform3D {
  rl::Vector3 translation = {0, 0, 0};
  rl::Quaternion rotation = rl::QuaternionIdentity();
  rl::Vector3 scale = {1, 1, 1};

  static Transform3D from_xyz(float x, float y, float z) {
    return {{x, y, z}, rl::QuaternionIdentity(), {1, 1, 1}};
  }

  static Transform3D from_translation(rl::Vector3 pos) {
    return {pos, rl::QuaternionIdentity(), {1, 1, 1}};
  }

  static Transform3D from_rotation(rl::Quaternion q) {
    return {{0, 0, 0}, q, {1, 1, 1}};
  }

  static Transform3D from_scale(rl::Vector3 s) {
    return {{0, 0, 0}, rl::QuaternionIdentity(), s};
  }

  static Transform3D from_scale_uniform(float s) {
    return from_scale({s, s, s});
  }

  rl::Matrix compute_matrix() const {
    rl::Matrix s = rl::MatrixScale(scale.x, scale.y, scale.z);
    rl::Matrix r = rl::QuaternionToMatrix(rotation);
    rl::Matrix t =
        rl::MatrixTranslate(translation.x, translation.y, translation.z);
    return rl::MatrixMultiply(rl::MatrixMultiply(s, r), t);
  }

  rl::Vector3 forward() const {
    return rl::Vector3Normalize(
        rl::Vector3RotateByQuaternion({0, 0, -1}, rotation));
  }

  rl::Vector3 right() const {
    return rl::Vector3Normalize(
        rl::Vector3RotateByQuaternion({1, 0, 0}, rotation));
  }

  rl::Vector3 up() const {
    return rl::Vector3Normalize(
        rl::Vector3RotateByQuaternion({0, 1, 0}, rotation));
  }

  Transform3D &look_at(rl::Vector3 target, rl::Vector3 world_up = {0, 1, 0}) {
    rl::Vector3 dir = rl::Vector3Subtract(target, translation);
    if (rl::Vector3LengthSqr(dir) < 1e-10f)
      return *this;

    rl::Matrix look = rl::MatrixLookAt(translation, target, world_up);
    rl::Matrix inv = rl::MatrixInvert(look);
    inv.m12 = 0;
    inv.m13 = 0;
    inv.m14 = 0;
    rotation = rl::QuaternionNormalize(rl::QuaternionFromMatrix(inv));
    return *this;
  }
};

struct GlobalTransform {
  rl::Matrix matrix = rl::MatrixIdentity();

  static GlobalTransform from_matrix(rl::Matrix m) { return {m}; }

  rl::Vector3 translation() const {
    return {matrix.m12, matrix.m13, matrix.m14};
  }

  rl::Vector3 forward() const {
    return rl::Vector3Normalize({-matrix.m8, -matrix.m9, -matrix.m10});
  }

  rl::Vector3 right() const {
    return rl::Vector3Normalize({matrix.m0, matrix.m1, matrix.m2});
  }

  rl::Vector3 up() const {
    return rl::Vector3Normalize({matrix.m4, matrix.m5, matrix.m6});
  }
};

inline void propagate_transforms(
    Query<Entity, Transform3D, Opt<Parent>, Mut<GlobalTransform>> query) {
  std::unordered_map<Entity, rl::Matrix> local_matrices;
  std::unordered_map<Entity, Entity> parents;

  for (auto [entity, transform, parent, global] : query.iter()) {
    local_matrices[entity] = transform->compute_matrix();
    if (parent)
      parents[entity] = parent->entity;
  }

  std::unordered_map<Entity, GlobalTransform> cache;
  std::function<GlobalTransform(Entity)> compute =
      [&](Entity entity) -> GlobalTransform {
    auto it = cache.find(entity);
    if (it != cache.end())
      return it->second;

    rl::Matrix local_mat = rl::MatrixIdentity();
    auto lit = local_matrices.find(entity);
    if (lit != local_matrices.end())
      local_mat = lit->second;

    GlobalTransform result;
    auto pit = parents.find(entity);
    if (pit != parents.end()) {
      GlobalTransform parent_global = compute(pit->second);
      result.matrix = rl::MatrixMultiply(local_mat, parent_global.matrix);
    } else {
      result.matrix = local_mat;
    }

    cache[entity] = result;
    return result;
  };

  for (auto &[entity, _] : local_matrices)
    compute(entity);

  for (auto [entity, transform, parent, global] : query.iter()) {
    auto it = cache.find(entity);
    if (it != cache.end())
      global.bypass_change_detection() = it->second;
  }
}

} // namespace ecs
