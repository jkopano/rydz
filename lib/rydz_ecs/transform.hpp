#pragma once
#include "entity.hpp"
#include "hierarchy.hpp"
#include "world.hpp"
#include <functional>
#include <raylib-cpp/raylib-cpp.hpp>
#include <unordered_map>

namespace ecs {

struct Transform3D {
  Vector3 translation = {0, 0, 0};
  Quaternion rotation = QuaternionIdentity();
  Vector3 scale = {1, 1, 1};

  static Transform3D from_xyz(float x, float y, float z) {
    return {{x, y, z}, QuaternionIdentity(), {1, 1, 1}};
  }

  static Transform3D from_translation(Vector3 pos) {
    return {pos, QuaternionIdentity(), {1, 1, 1}};
  }

  static Transform3D from_rotation(Quaternion q) {
    return {{0, 0, 0}, q, {1, 1, 1}};
  }

  static Transform3D from_scale(Vector3 s) {
    return {{0, 0, 0}, QuaternionIdentity(), s};
  }

  static Transform3D from_scale_uniform(float s) {
    return from_scale({s, s, s});
  }

  Matrix compute_matrix() const {
    Matrix s = MatrixScale(scale.x, scale.y, scale.z);
    Matrix r = QuaternionToMatrix(rotation);
    Matrix t = MatrixTranslate(translation.x, translation.y, translation.z);
    return MatrixMultiply(MatrixMultiply(s, r), t);
  }

  Vector3 forward() const {
    return Vector3Normalize(Vector3RotateByQuaternion({0, 0, -1}, rotation));
  }

  Vector3 right() const {
    return Vector3Normalize(Vector3RotateByQuaternion({1, 0, 0}, rotation));
  }

  Vector3 up() const {
    return Vector3Normalize(Vector3RotateByQuaternion({0, 1, 0}, rotation));
  }

  Transform3D &look_at(Vector3 target, Vector3 world_up = {0, 1, 0}) {
    Vector3 dir = Vector3Subtract(target, translation);
    if (Vector3LengthSqr(dir) < 1e-10f)
      return *this;

    Matrix look = MatrixLookAt(translation, target, world_up);
    Matrix inv = MatrixInvert(look);
    inv.m12 = 0;
    inv.m13 = 0;
    inv.m14 = 0;
    rotation = QuaternionNormalize(QuaternionFromMatrix(inv));
    return *this;
  }
};

struct GlobalTransform {
  Matrix matrix = MatrixIdentity();

  static GlobalTransform from_matrix(Matrix m) { return {m}; }

  Vector3 translation() const { return {matrix.m12, matrix.m13, matrix.m14}; }

  Vector3 forward() const {
    return Vector3Normalize({-matrix.m8, -matrix.m9, -matrix.m10});
  }

  Vector3 right() const {
    return Vector3Normalize({matrix.m0, matrix.m1, matrix.m2});
  }

  Vector3 up() const {
    return Vector3Normalize({matrix.m4, matrix.m5, matrix.m6});
  }
};

inline void propagate_transforms(World &world) {
  std::unordered_map<Entity, Matrix> local_matrices;
  auto *transform_storage = world.get_vec_storage<Transform3D>();
  if (transform_storage) {
    for (auto e : transform_storage->entity_indices()) {
      auto *t = transform_storage->get(e);
      if (t)
        local_matrices[e] = t->compute_matrix();
    }
  }

  std::unordered_map<Entity, Entity> parents;
  auto *parent_storage = world.get_vec_storage<Parent>();
  if (parent_storage) {
    for (auto e : parent_storage->entity_indices()) {
      auto *p = parent_storage->get(e);
      if (p)
        parents[e] = p->entity;
    }
  }

  std::unordered_map<Entity, GlobalTransform> cache;
  std::function<GlobalTransform(Entity)> compute =
      [&](Entity entity) -> GlobalTransform {
    auto it = cache.find(entity);
    if (it != cache.end())
      return it->second;

    Matrix local_mat = MatrixIdentity();
    auto lit = local_matrices.find(entity);
    if (lit != local_matrices.end())
      local_mat = lit->second;

    GlobalTransform global;
    auto pit = parents.find(entity);
    if (pit != parents.end()) {
      GlobalTransform parent_global = compute(pit->second);
      global.matrix = MatrixMultiply(local_mat, parent_global.matrix);
    } else {
      global.matrix = local_mat;
    }

    cache[entity] = global;
    return global;
  };

  for (auto &[entity, _] : local_matrices) {
    compute(entity);
  }

  for (auto &[entity, global] : cache) {
    world.insert_component(entity, global);
  }
}

} // namespace ecs
