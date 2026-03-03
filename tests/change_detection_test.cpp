#include <gtest/gtest.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Components
// ============================================================

struct Health {
  int value;
  bool operator==(const Health &o) const { return value == o.value; }
};

struct Mana {
  int value;
  bool operator==(const Mana &o) const { return value == o.value; }
};

// ============================================================
// Change Detection tests (mirrors change_detection_test.rs)
// ============================================================

TEST(ChangeDetectionTest, AddedDetectsNewComponent) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  // On insertion tick, Added should match
  EXPECT_TRUE((QueryFilterTraits<Added<Health>>::matches(world, entity)));

  // After incrementing tick, Added should no longer match
  world.increment_change_tick();
  EXPECT_FALSE((QueryFilterTraits<Added<Health>>::matches(world, entity)));
}

TEST(ChangeDetectionTest, ChangedDetectsMutation) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  // Move past insertion tick
  world.increment_change_tick();
  EXPECT_FALSE((QueryFilterTraits<Changed<Health>>::matches(world, entity)));

  // Mutate the component (must mark changed)
  auto &storage = world.ensure_vec_storage<Health>();
  storage.mark_changed(entity, world.read_change_tick());

  // Changed should now match
  EXPECT_TRUE((QueryFilterTraits<Changed<Health>>::matches(world, entity)));

  // After incrementing tick, Changed should no longer match
  world.increment_change_tick();
  EXPECT_FALSE((QueryFilterTraits<Changed<Health>>::matches(world, entity)));
}

TEST(ChangeDetectionTest, AddedAndChangedOnInsert) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  // Both should match on insertion frame
  EXPECT_TRUE((QueryFilterTraits<Added<Health>>::matches(world, entity)));
  EXPECT_TRUE((QueryFilterTraits<Changed<Health>>::matches(world, entity)));
}

TEST(ChangeDetectionTest, AddedOnlyOnInsertionFrame) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  EXPECT_TRUE((QueryFilterTraits<Added<Health>>::matches(world, entity)));

  for (int i = 0; i < 5; i++) {
    world.increment_change_tick();
    EXPECT_FALSE((QueryFilterTraits<Added<Health>>::matches(world, entity)));
  }
}

TEST(ChangeDetectionTest, QueryWithAddedFilter) {
  World world;

  // Create entities at tick 0
  auto e1 = world.spawn();
  world.insert_component(e1, Health{100});
  auto e2 = world.spawn();
  world.insert_component(e2, Health{200});

  world.increment_change_tick();

  // Create entity at tick 1
  auto e3 = world.spawn();
  world.insert_component(e3, Health{300});

  // Query with Added<Health> should only return e3
  std::vector<int> results;
  Query<Health, Filters<Added<Health>>> query(world);
  query.for_each([&](const Health *h) { results.push_back(h->value); });

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], 300);
}

TEST(ChangeDetectionTest, QueryWithChangedFilter) {
  World world;

  auto e1 = world.spawn();
  world.insert_component(e1, Health{100});
  auto e2 = world.spawn();
  world.insert_component(e2, Health{200});
  auto e3 = world.spawn();
  world.insert_component(e3, Health{300});

  world.increment_change_tick();

  // Mutate only e2
  auto &storage = world.ensure_vec_storage<Health>();
  storage.mark_changed(e2, world.read_change_tick());

  // Query with Changed<Health> should only return e2
  std::vector<int> results;
  Query<Health, Filters<Changed<Health>>> query(world);
  query.for_each([&](const Health *h) { results.push_back(h->value); });

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], 200);
}

TEST(ChangeDetectionTest, AddedNoComponent) {
  World world;
  auto entity = world.spawn();
  // No component inserted
  EXPECT_FALSE((QueryFilterTraits<Added<Health>>::matches(world, entity)));
}

TEST(ChangeDetectionTest, ChangedNoComponent) {
  World world;
  auto entity = world.spawn();
  // No component inserted
  EXPECT_FALSE((QueryFilterTraits<Changed<Health>>::matches(world, entity)));
}
