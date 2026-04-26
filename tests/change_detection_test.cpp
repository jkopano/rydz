#include <gtest/gtest.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

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

} // namespace

// ============================================================
// Change Detection tests (mirrors change_detection_test.rs)
// ============================================================

TEST(ChangeDetectionTest, AddedDetectsNewComponent) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  Tick this_run = world.read_change_tick();

  // On insertion tick, Added should match (last_run=0 means "never ran before")
  EXPECT_TRUE((QueryFilterTraits<Added<Health>>::matches(world, entity, Tick{0}, this_run)));

  // After incrementing tick, Added should no longer match when last_run is the
  // previous tick
  world.increment_change_tick();
  Tick new_this_run = world.read_change_tick();
  EXPECT_FALSE(
      (QueryFilterTraits<Added<Health>>::matches(world, entity, this_run, new_this_run)));
}

TEST(ChangeDetectionTest, ChangedDetectsMutation) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  Tick last_run = world.read_change_tick();

  // Move past insertion tick
  world.increment_change_tick();
  Tick this_run = world.read_change_tick();
  EXPECT_FALSE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));

  // Mutate the component (must mark changed)
  auto &storage = world.ensure_storage_exist<Health>();
  storage.mark_changed(entity, this_run);

  // Changed should now match
  EXPECT_TRUE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));

  // After incrementing tick, Changed should no longer match
  last_run = this_run;
  world.increment_change_tick();
  this_run = world.read_change_tick();
  EXPECT_FALSE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));
}

TEST(ChangeDetectionTest, AddedAndChangedOnInsert) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  Tick this_run = world.read_change_tick();

  // Both should match on insertion frame
  EXPECT_TRUE(
      (QueryFilterTraits<Added<Health>>::matches(world, entity, Tick{0}, this_run)));
  EXPECT_TRUE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, Tick{0}, this_run)));
}

TEST(ChangeDetectionTest, AddedOnlyOnInsertionFrame) {
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  Tick last_run{0};
  Tick this_run = world.read_change_tick();

  EXPECT_TRUE(
      (QueryFilterTraits<Added<Health>>::matches(world, entity, last_run, this_run)));

  for (int i = 0; i < 5; i++) {
    last_run = this_run;
    world.increment_change_tick();
    this_run = world.read_change_tick();
    EXPECT_FALSE(
        (QueryFilterTraits<Added<Health>>::matches(world, entity, last_run, this_run)));
  }
}

TEST(ChangeDetectionTest, QueryWithAddedFilter) {
  World world;

  // Create entities at tick 0
  auto e1 = world.spawn();
  world.insert_component(e1, Health{100});
  auto e2 = world.spawn();
  world.insert_component(e2, Health{200});

  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  Tick this_run = world.read_change_tick();

  // Create entity at tick 1
  auto e3 = world.spawn();
  world.insert_component(e3, Health{300});

  // Query with Added<Health> should only return e3
  std::vector<int> results;
  Query<Health, Filters<Added<Health>>> query(world, last_run, this_run);
  query.each([&](const Health *h) { results.push_back(h->value); });

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

  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  Tick this_run = world.read_change_tick();

  // Mutate only e2
  auto &storage = world.ensure_storage_exist<Health>();
  storage.mark_changed(e2, this_run);

  // Query with Changed<Health> should only return e2
  std::vector<int> results;
  Query<Health, Filters<Changed<Health>>> query(world, last_run, this_run);
  query.each([&](const Health *h) { results.push_back(h->value); });

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], 200);
}

TEST(ChangeDetectionTest, AddedNoComponent) {
  World world;
  auto entity = world.spawn();
  // No component inserted
  EXPECT_FALSE((QueryFilterTraits<Added<Health>>::matches(
      world, entity, Tick{0}, world.read_change_tick())));
}

TEST(ChangeDetectionTest, ChangedNoComponent) {
  World world;
  auto entity = world.spawn();
  // No component inserted
  EXPECT_FALSE((QueryFilterTraits<Changed<Health>>::matches(
      world, entity, Tick{0}, world.read_change_tick())));
}

TEST(ChangeDetectionTest, AddedDetectsAcrossTickGap) {
  // This is the startup scenario: component added at tick 0, system first runs
  // at tick 2 (having never run before, last_run = 0)
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100}); // added at tick 0

  world.increment_change_tick(); // tick 1
  world.increment_change_tick(); // tick 2

  Tick this_run = world.read_change_tick(); // tick 2
  Tick last_run{0};                         // system never ran before

  // Should still detect the added component because last_run is 0
  EXPECT_TRUE(
      (QueryFilterTraits<Added<Health>>::matches(world, entity, last_run, this_run)));
}

TEST(ChangeDetectionTest, ChangedDetectsAcrossMultipleRuns) {
  // Symuluje scenariusz z UI: system uruchamia się wielokrotnie,
  // komponent jest modyfikowany między uruchomieniami
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  // Pierwsze uruchomienie systemu (last_run = 0)
  Tick last_run{0};
  Tick this_run = world.read_change_tick();

  // Komponent został dodany, więc Changed powinien wykryć
  EXPECT_TRUE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));

  // Drugie uruchomienie - bez zmian
  last_run = this_run;
  world.increment_change_tick();
  this_run = world.read_change_tick();

  EXPECT_FALSE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));

  // Trzecie uruchomienie - z modyfikacją między uruchomieniami
  last_run = this_run;
  world.increment_change_tick();
  this_run = world.read_change_tick();

  auto &storage = world.ensure_storage_exist<Health>();
  storage.mark_changed(entity, this_run);

  EXPECT_TRUE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));

  // Czwarte uruchomienie - bez nowych zmian
  last_run = this_run;
  world.increment_change_tick();
  this_run = world.read_change_tick();

  EXPECT_FALSE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, last_run, this_run)));
}

TEST(ChangeDetectionTest, ChangedWithMutQuery) {
  // Test integracyjny: Changed<T> z Query<Mut<T>>
  World world;

  auto e1 = world.spawn();
  world.insert_component(e1, Health{100});
  auto e2 = world.spawn();
  world.insert_component(e2, Health{200});

  Tick tick0 = world.read_change_tick();
  world.increment_change_tick();
  Tick tick1 = world.read_change_tick();

  // Modyfikujemy e1 przez Mut<Health> w tick1
  // Use Entity to identify which entity to mutate, because any non-const
  // access through Mut<T>::operator->() triggers mark() (C++ can't
  // distinguish reads from writes in operator->).
  {
    Query<Entity, Mut<Health>> mut_query(world, tick0, tick1);
    mut_query.each([&](Entity entity, Mut<Health> h) {
      if (entity == e1) {
        h->value = 150; // To powinno oznaczyć jako changed w tick1
      }
    });
  }

  // Nowe uruchomienie w tick2 - Changed powinien wykryć e1 (zmieniony w tick1)
  // last_run=tick0 (ostatnie uruchomienie systemu), this_run=tick2
  world.increment_change_tick();
  Tick tick2 = world.read_change_tick();

  std::vector<int> changed_values;
  Query<Health, Filters<Changed<Health>>> changed_query(world, tick0, tick2);
  changed_query.each([&](const Health *h) { changed_values.push_back(h->value); });

  ASSERT_EQ(changed_values.size(), 1u);
  EXPECT_EQ(changed_values[0], 150);
}

TEST(ChangeDetectionTest, ChangedDoesNotDetectOldChanges) {
  // Test że Changed nie wykrywa zmian sprzed last_run
  World world;
  auto entity = world.spawn();
  world.insert_component(entity, Health{100});

  Tick tick0 = world.read_change_tick();
  world.increment_change_tick();
  Tick tick1 = world.read_change_tick();

  // Modyfikacja w tick1
  auto &storage = world.ensure_storage_exist<Health>();
  storage.mark_changed(entity, tick1);

  world.increment_change_tick();
  Tick tick2 = world.read_change_tick();
  world.increment_change_tick();
  Tick tick3 = world.read_change_tick();

  // System uruchamia się w tick3, ale last_run = tick2
  // Zmiana była w tick1, więc nie powinna być wykryta
  EXPECT_FALSE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, tick2, tick3)));

  // Ale jeśli last_run = tick0, to zmiana powinna być wykryta
  EXPECT_TRUE(
      (QueryFilterTraits<Changed<Health>>::matches(world, entity, tick0, tick3)));
}
