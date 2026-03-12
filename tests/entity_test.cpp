#include <gtest/gtest.h>

#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// EntityManager tests (mirrors entity_test.rs)
// ============================================================

TEST(EntityTest, EntityCreation) {
  EntityManager manager;
  auto e1 = manager.create();
  auto e2 = manager.create();

  EXPECT_EQ(e1.index(), 0u);
  EXPECT_EQ(e2.index(), 1u);
  EXPECT_NE(e1, e2);
}

TEST(EntityTest, EntityTraits) {
  auto e1 = Entity::from_raw(10, 0);
  auto e2 = Entity::from_raw(10, 0);
  auto e3 = Entity::from_raw(20, 0);

  EXPECT_EQ(e1, e2);
  EXPECT_NE(e1, e3);
  EXPECT_LT(e1, e3);

  auto e4 = e1; // copy
  EXPECT_EQ(e1, e4);
}

TEST(EntityTest, EntityRecycling) {
  EntityManager manager;

  auto e1 = manager.create();
  auto e2 = manager.create();
  EXPECT_EQ(e1.index(), 0u);
  EXPECT_EQ(e1.generation(), 0u);
  EXPECT_EQ(e2.index(), 1u);

  manager.destroy(e1);
  manager.destroy(e2);

  // LIFO: e2 (id 1) was freed last, so it comes out first
  auto e3 = manager.create();
  EXPECT_EQ(e3.index(), 1u);
  EXPECT_EQ(e3.generation(), 1u);
  EXPECT_NE(e1, e3);

  auto e4 = manager.create();
  EXPECT_EQ(e4.index(), 0u);
  EXPECT_EQ(e4.generation(), 1u);
}

// ============================================================
// World-level entity tests
// ============================================================

struct TestComponent {
  uint32_t value;
  bool operator==(const TestComponent &o) const { return value == o.value; }
};

TEST(EntityTest, GenerationPreventsStaleAccess) {
  World world;

  auto e1 = world.spawn();
  world.insert_component(e1, TestComponent{42});
  EXPECT_NE(world.get_component<TestComponent>(e1), nullptr);
  EXPECT_EQ(world.get_component<TestComponent>(e1)->value, 42u);

  world.despawn(e1);

  EXPECT_EQ(world.get_component<TestComponent>(e1), nullptr);

  auto e2 = world.spawn();
  EXPECT_EQ(e2.index(), e1.index());
  EXPECT_NE(e2.generation(), e1.generation());

  // New entity should not have the component
  EXPECT_EQ(world.get_component<TestComponent>(e2), nullptr);

  world.insert_component(e2, TestComponent{99});
  EXPECT_NE(world.get_component<TestComponent>(e2), nullptr);
  EXPECT_EQ(world.get_component<TestComponent>(e2)->value, 99u);
}
