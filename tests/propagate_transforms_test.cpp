#include <gtest/gtest.h>

#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include "rydz_math/mod.hpp"

using namespace ecs;
namespace math = rydz_math;

// Helper: run propagate_transforms on a World
static void run_propagate(World& world) {
  Query<Entity, Transform, Opt<Parent>, Mut<GlobalTransform>> q(world);
  propagate_transforms(q);
}

// Helper: get GlobalTransform matrix for an entity
static math::Mat4 global_matrix(World& world, Entity e) {
  auto* gt = world.get_component<GlobalTransform>(e);
  EXPECT_NE(gt, nullptr);
  return gt ? gt->matrix : math::Mat4::IDENTITY;
}

// ─── Unit test: root → child → grandchild ────────────────────────────────────
// Requirements: 5.3

TEST(PropagateTransformsTest, RootChildGrandchildHierarchy) {
  World world;

  // root: translate (1, 0, 0)
  Entity root = world.spawn();
  world.insert_component(root, Transform::from_xyz(1.0F, 0.0F, 0.0F));

  // child: translate (0, 2, 0), parent = root
  Entity child = world.spawn();
  world.insert_component(child, Transform::from_xyz(0.0F, 2.0F, 0.0F));
  world.insert_component(child, Parent{root});

  // grandchild: translate (0, 0, 3), parent = child
  Entity grandchild = world.spawn();
  world.insert_component(grandchild, Transform::from_xyz(0.0F, 0.0F, 3.0F));
  world.insert_component(grandchild, Parent{child});

  run_propagate(world);

  // root global == local
  math::Mat4 root_global = global_matrix(world, root);
  EXPECT_NEAR(root_global.translation().x, 1.0F, 1e-5F);
  EXPECT_NEAR(root_global.translation().y, 0.0F, 1e-5F);
  EXPECT_NEAR(root_global.translation().z, 0.0F, 1e-5F);

  // child global == root_global * child_local → (1, 2, 0)
  math::Mat4 child_global = global_matrix(world, child);
  EXPECT_NEAR(child_global.translation().x, 1.0F, 1e-5F);
  EXPECT_NEAR(child_global.translation().y, 2.0F, 1e-5F);
  EXPECT_NEAR(child_global.translation().z, 0.0F, 1e-5F);

  // grandchild global == child_global * grandchild_local → (1, 2, 3)
  math::Mat4 gc_global = global_matrix(world, grandchild);
  EXPECT_NEAR(gc_global.translation().x, 1.0F, 1e-5F);
  EXPECT_NEAR(gc_global.translation().y, 2.0F, 1e-5F);
  EXPECT_NEAR(gc_global.translation().z, 3.0F, 1e-5F);
}

TEST(PropagateTransformsTest, RootWithNoParentUsesLocalTransform) {
  World world;
  Entity root = world.spawn();
  world.insert_component(root, Transform::from_xyz(5.0F, 6.0F, 7.0F));

  run_propagate(world);

  math::Mat4 g = global_matrix(world, root);
  EXPECT_NEAR(g.translation().x, 5.0F, 1e-5F);
  EXPECT_NEAR(g.translation().y, 6.0F, 1e-5F);
  EXPECT_NEAR(g.translation().z, 7.0F, 1e-5F);
}

// ─── Property test: idempotency ───────────────────────────────────────────────
// **Validates: Requirements 5.3**
// Running propagate_transforms twice produces identical GlobalTransform values.

TEST(PropagateTransformsTest, IdempotencyOnFlatHierarchy) {
  // Property: for any set of root entities, running twice gives same result
  for (int seed = 0; seed < 50; ++seed) {
    World world;
    float base = static_cast<float>(seed);

    Entity e1 = world.spawn();
    world.insert_component(e1, Transform::from_xyz(base, base + 1.0F, base + 2.0F));

    Entity e2 = world.spawn();
    world.insert_component(e2, Transform::from_xyz(-base, base * 0.5F, 0.0F));

    run_propagate(world);
    math::Mat4 g1_first = global_matrix(world, e1);
    math::Mat4 g2_first = global_matrix(world, e2);

    run_propagate(world);
    math::Mat4 g1_second = global_matrix(world, e1);
    math::Mat4 g2_second = global_matrix(world, e2);

    // All 16 matrix elements must be identical
    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        EXPECT_FLOAT_EQ(g1_first[col][row], g1_second[col][row])
          << "seed=" << seed << " e1 col=" << col << " row=" << row;
        EXPECT_FLOAT_EQ(g2_first[col][row], g2_second[col][row])
          << "seed=" << seed << " e2 col=" << col << " row=" << row;
      }
    }
  }
}

TEST(PropagateTransformsTest, IdempotencyOnDeepHierarchy) {
  // Property: deep chain — running twice gives same result
  // **Validates: Requirements 5.3**
  for (int depth = 1; depth <= 20; ++depth) {
    World world;

    Entity prev = world.spawn();
    world.insert_component(prev, Transform::from_xyz(1.0F, 0.0F, 0.0F));

    for (int i = 1; i < depth; ++i) {
      Entity cur = world.spawn();
      world.insert_component(cur, Transform::from_xyz(1.0F, 0.0F, 0.0F));
      world.insert_component(cur, Parent{prev});
      prev = cur;
    }

    run_propagate(world);
    math::Mat4 leaf_first = global_matrix(world, prev);

    run_propagate(world);
    math::Mat4 leaf_second = global_matrix(world, prev);

    for (int col = 0; col < 4; ++col) {
      for (int row = 0; row < 4; ++row) {
        EXPECT_FLOAT_EQ(leaf_first[col][row], leaf_second[col][row])
          << "depth=" << depth << " col=" << col << " row=" << row;
      }
    }
  }
}

TEST(PropagateTransformsTest, DeepChainAccumulatesTranslation) {
  // Verify correctness: chain of N nodes each translating (1,0,0)
  // → leaf global translation == (N, 0, 0)
  constexpr int N = 10;
  World world;

  Entity prev = world.spawn();
  world.insert_component(prev, Transform::from_xyz(1.0F, 0.0F, 0.0F));

  for (int i = 1; i < N; ++i) {
    Entity cur = world.spawn();
    world.insert_component(cur, Transform::from_xyz(1.0F, 0.0F, 0.0F));
    world.insert_component(cur, Parent{prev});
    prev = cur;
  }

  run_propagate(world);

  math::Mat4 leaf = global_matrix(world, prev);
  EXPECT_NEAR(leaf.translation().x, static_cast<float>(N), 1e-4F);
  EXPECT_NEAR(leaf.translation().y, 0.0F, 1e-4F);
  EXPECT_NEAR(leaf.translation().z, 0.0F, 1e-4F);
}
