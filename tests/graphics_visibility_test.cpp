#include <gtest/gtest.h>

#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_graphics/spatial/frustum.hpp"
#include "rydz_graphics/spatial/visibility.hpp"

using namespace ecs;

namespace {

const ComputedVisibility *computed_visibility(World &world, Entity entity) {
  return world.get_component<ComputedVisibility>(entity);
}

} // namespace

TEST(GraphicsVisibilityTest, HiddenParentHidesChildWithoutVisibility) {
  World world;
  Entity parent = world.spawn();
  Entity child = world.spawn();

  world.insert_component(parent, Visibility::Hidden);
  world.insert_component(child, Parent{parent});

  compute_visibility(world);

  const auto *parent_visibility = computed_visibility(world, parent);
  const auto *child_visibility = computed_visibility(world, child);
  ASSERT_NE(parent_visibility, nullptr);
  ASSERT_NE(child_visibility, nullptr);
  EXPECT_FALSE(parent_visibility->visible);
  EXPECT_FALSE(child_visibility->visible);
}

TEST(GraphicsVisibilityTest, ExplicitVisibleChildOverridesHiddenParent) {
  World world;
  Entity parent = world.spawn();
  Entity child = world.spawn();

  world.insert_component(parent, Visibility::Hidden);
  world.insert_component(child, Parent{parent});
  world.insert_component(child, Visibility::Visible);

  compute_visibility(world);

  const auto *child_visibility = computed_visibility(world, child);
  ASSERT_NE(child_visibility, nullptr);
  EXPECT_TRUE(child_visibility->visible);
}

TEST(GraphicsVisibilityTest, HierarchyWithoutVisibilityStillComputesVisible) {
  World world;
  Entity parent = world.spawn();
  Entity child = world.spawn();

  world.insert_component(child, Parent{parent});

  compute_visibility(world);

  const auto *parent_visibility = computed_visibility(world, parent);
  const auto *child_visibility = computed_visibility(world, child);
  ASSERT_NE(parent_visibility, nullptr);
  ASSERT_NE(child_visibility, nullptr);
  EXPECT_TRUE(parent_visibility->visible);
  EXPECT_TRUE(child_visibility->visible);
}

TEST(GraphicsVisibilityTest, ParentVisibilityChangeRefreshesChildState) {
  World world;
  Entity parent = world.spawn();
  Entity child = world.spawn();

  world.insert_component(parent, Visibility::Hidden);
  world.insert_component(child, Parent{parent});
  compute_visibility(world);

  auto *child_visibility = world.get_component<ComputedVisibility>(child);
  ASSERT_NE(child_visibility, nullptr);
  ASSERT_FALSE(child_visibility->visible);

  world.insert_component(parent, Visibility::Visible);
  compute_visibility(world);

  child_visibility = world.get_component<ComputedVisibility>(child);
  ASSERT_NE(child_visibility, nullptr);
  EXPECT_TRUE(child_visibility->visible);
}

// ─── Unit test 8.2: Hidden parent propagation ────────────────────────────────
// Requirements: 5.4

TEST(GraphicsVisibilityTest, HiddenParentForcesAllDescendantsInvisible) {
  World world;
  Entity parent = world.spawn();
  Entity child = world.spawn();
  Entity grandchild = world.spawn();

  world.insert_component(parent, Visibility::Hidden);
  world.insert_component(child, Parent{parent});
  world.insert_component(grandchild, Parent{child});

  compute_visibility(world);

  const auto *parent_cv = computed_visibility(world, parent);
  const auto *child_cv = computed_visibility(world, child);
  const auto *gc_cv = computed_visibility(world, grandchild);

  ASSERT_NE(parent_cv, nullptr);
  ASSERT_NE(child_cv, nullptr);
  ASSERT_NE(gc_cv, nullptr);

  EXPECT_FALSE(parent_cv->visible);
  EXPECT_FALSE(child_cv->visible);
  EXPECT_FALSE(gc_cv->visible);
}

// ─── Property test 8.1: Visibility monotonicity ──────────────────────────────
// **Validates: Requirements 5.4**
// If any ancestor has Visibility::Hidden, then ComputedVisibility::visible ==
// false for all descendants.

TEST(GraphicsVisibilityTest, VisibilityMonotonicity_HiddenAncestorImpliesInvisibleDescendants) {
  // Sweep over chain depths 1..15; at each depth, mark the root Hidden and
  // verify every node in the chain is invisible.
  for (int depth = 1; depth <= 15; ++depth) {
    World world;

    std::vector<Entity> chain;
    chain.reserve(static_cast<std::size_t>(depth));

    Entity root = world.spawn();
    world.insert_component(root, Visibility::Hidden);
    chain.push_back(root);

    for (int i = 1; i < depth; ++i) {
      Entity cur = world.spawn();
      world.insert_component(cur, Parent{chain.back()});
      chain.push_back(cur);
    }

    compute_visibility(world);

    for (Entity e : chain) {
      const auto *cv = computed_visibility(world, e);
      ASSERT_NE(cv, nullptr) << "depth=" << depth;
      EXPECT_FALSE(cv->visible)
        << "depth=" << depth
        << " should be invisible because root is Hidden";
    }
  }
}

TEST(GraphicsVisibilityTest, VisibilityMonotonicity_HiddenMidChainImpliesInvisibleBelow) {
  // Mark a mid-chain node Hidden; nodes above it remain visible,
  // nodes at and below it must be invisible.
  for (int total = 3; total <= 10; ++total) {
    for (int hidden_idx = 1; hidden_idx < total; ++hidden_idx) {
      World world;

      std::vector<Entity> chain;
      chain.reserve(static_cast<std::size_t>(total));

      Entity root = world.spawn();
      chain.push_back(root);

      for (int i = 1; i < total; ++i) {
        Entity cur = world.spawn();
        world.insert_component(cur, Parent{chain.back()});
        chain.push_back(cur);
      }

      world.insert_component(chain[static_cast<std::size_t>(hidden_idx)],
                              Visibility::Hidden);

      compute_visibility(world);

      for (int i = 0; i < total; ++i) {
        const auto *cv = computed_visibility(world, chain[static_cast<std::size_t>(i)]);
        ASSERT_NE(cv, nullptr) << "total=" << total << " hidden_idx=" << hidden_idx;
        if (i >= hidden_idx) {
          EXPECT_FALSE(cv->visible)
            << "total=" << total << " hidden_idx=" << hidden_idx
            << " node " << i << " should be invisible";
        } else {
          EXPECT_TRUE(cv->visible)
            << "total=" << total << " hidden_idx=" << hidden_idx
            << " node " << i << " should be visible";
        }
      }
    }
  }
}

TEST(GraphicsVisibilityTest, SphereFrustumCullKeepsVisibleLights) {
  constexpr float HALF_PI = 1.57079632679F;
  math::Mat4 const projection =
    math::Mat4::perspective_rh(HALF_PI, 1.0F, 0.1F, 50.0F);
  auto const planes = extract_frustum_planes(projection);

  EXPECT_TRUE(sphere_in_frustum(math::Vec3{0.0F, 0.0F, -5.0F}, 1.0F, planes));
  EXPECT_TRUE(sphere_in_frustum(math::Vec3{6.0F, 0.0F, -5.0F}, 2.5F, planes));
}

TEST(GraphicsVisibilityTest, SphereFrustumCullRejectsOffscreenLights) {
  constexpr float HALF_PI = 1.57079632679F;
  math::Mat4 const projection =
    math::Mat4::perspective_rh(HALF_PI, 1.0F, 0.1F, 50.0F);
  auto const planes = extract_frustum_planes(projection);

  EXPECT_FALSE(sphere_in_frustum(math::Vec3{12.0F, 0.0F, -5.0F}, 1.0F, planes));
  EXPECT_FALSE(sphere_in_frustum(math::Vec3{0.0F, 0.0F, 5.0F}, 1.0F, planes));
}
