#pragma once
#include "math.hpp"
#include "rydz_ecs/hierarchy.hpp"
#include "rydz_ecs/query.hpp"
#include <functional>
#include <unordered_map>

namespace ecs {

using namespace math;

struct Transform {
  Vec3 translation = Vec3::sZero();
  Quat rotation = Quat::sIdentity();
  Vec3 scale = Vec3::sReplicate(1.0f);

  static Transform from_xyz(float x, float y, float z) {
    return {{Vec3(x, y, z)}, Quat::sIdentity(), Vec3::sReplicate(1.0f)};
  }

  static Transform from_translation(Vec3 pos) {
    return {pos, Quat::sIdentity(), Vec3::sReplicate(1.0f)};
  }

  static Transform from_rotation(Quat q) {
    return {Vec3::sZero(), q, Vec3::sReplicate(1.0f)};
  }

  static Transform from_scale(Vec3 s) {
    return {Vec3::sZero(), Quat::sIdentity(), s};
  }

  static Transform from_scale_uniform(float s) {
    return from_scale(Vec3::sReplicate(s));
  }

  Mat4 compute_matrix() const {
    return Mat4::sRotationTranslation(rotation, translation).PreScaled(scale);
  }

  Vec3 forward() const { return rotation * Vec3(0, 0, -1); }

  Vec3 right() const { return rotation * Vec3(1, 0, 0); }

  Vec3 up() const { return rotation * Vec3(0, 1, 0); }

  Transform &look_at(Vec3 target, Vec3 world_up = Vec3(0, 1, 0)) {
    Vec3 dir = target - translation;
    if (dir.LengthSq() < 1e-10f)
      return *this;

    Mat4 look = Mat4::sLookAt(translation, target, world_up);
    Mat4 inv = look.Inversed();
    inv.SetTranslation(Vec3::sZero());
    rotation = inv.GetQuaternion().Normalized();
    return *this;
  }
};

struct GlobalTransform {
  Mat4 matrix = Mat4::sIdentity();

  static GlobalTransform from_matrix(Mat4 m) { return {m}; }

  Vec3 translation() const { return matrix.GetTranslation(); }

  Vec3 forward() const { return (-matrix.GetAxisZ()).Normalized(); }

  Vec3 right() const { return matrix.GetAxisX().Normalized(); }

  Vec3 up() const { return matrix.GetAxisY().Normalized(); }
};

inline void propagate_transforms(
    Query<Entity, Transform, Opt<Parent>, Mut<GlobalTransform>> query) {

  std::unordered_map<Entity, Mat4> local_matrices;
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

    Mat4 local_mat = Mat4::sIdentity();
    auto lit = local_matrices.find(entity);
    if (lit != local_matrices.end())
      local_mat = lit->second;

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

  for (auto &[entity, _] : local_matrices)
    compute(entity);

  for (auto [entity, transform, parent, global] : query.iter()) {
    auto it = cache.find(entity);
    if (it != cache.end())
      global.bypass_change_detection() = it->second;
  }
}

} // namespace ecs
