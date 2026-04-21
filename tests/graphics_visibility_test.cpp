#include <gtest/gtest.h>

#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_graphics/visibility.hpp"

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
