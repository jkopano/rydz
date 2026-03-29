#include <gtest/gtest.h>

#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

struct Counter {
  using T = Resource;
  int value = 0;
};

struct Log {
  using T = Resource;
  std::vector<std::string> entries;
};

// Sets — enum-based
enum class PhysicsSet { Movement, Collision };
enum class GameSet { Input, Logic, Render };

// Sets — struct-based
struct AudioSet {
  using T = Set;
};

// Named systems for ordering verification
void sys_movement(ResMut<Log> log) { log->entries.push_back("movement"); }
void sys_collision(ResMut<Log> log) { log->entries.push_back("collision"); }
void sys_input(ResMut<Log> log) { log->entries.push_back("input"); }
void sys_logic(ResMut<Log> log) { log->entries.push_back("logic"); }
void sys_render(ResMut<Log> log) { log->entries.push_back("render"); }
void sys_audio(ResMut<Log> log) { log->entries.push_back("audio"); }

} // namespace

// ============================================================
// Basic set ordering
// ============================================================

TEST(SystemSetTest, SetBeforeEnforcesOrder) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_collision).in_set(set(PhysicsSet::Collision))));
  schedule.add_system_fn(
      std::move(group(sys_movement).in_set(set(PhysicsSet::Movement))));

  // Movement before Collision
  schedule.add_set_config(
      configure(PhysicsSet::Movement).before(set(PhysicsSet::Collision)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0], "movement");
  EXPECT_EQ(entries[1], "collision");
}

TEST(SystemSetTest, SetAfterEnforcesOrder) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_render).in_set(set(GameSet::Render))));
  schedule.add_system_fn(
      std::move(group(sys_logic).in_set(set(GameSet::Logic))));

  // Render after Logic
  schedule.add_set_config(
      configure(GameSet::Render).after(set(GameSet::Logic)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0], "logic");
  EXPECT_EQ(entries[1], "render");
}

// ============================================================
// Multiple systems in a set
// ============================================================

TEST(SystemSetTest, AllSystemsInSetRunBeforeOtherSet) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  // Two systems in Input set
  schedule.add_system_fn(
      std::move(group(sys_input).in_set(set(GameSet::Input))));
  schedule.add_system_fn(
      std::move(group(sys_movement).in_set(set(GameSet::Input))));

  // One system in Logic set
  schedule.add_system_fn(
      std::move(group(sys_logic).in_set(set(GameSet::Logic))));

  // Input before Logic
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 3u);
  // logic must be last
  EXPECT_EQ(entries[2], "logic");
  // input and movement can be in any order, but both before logic
}

// ============================================================
// Struct-based sets
// ============================================================

TEST(SystemSetTest, StructBasedSet) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_audio).in_set(set<AudioSet>())));
  schedule.add_system_fn(
      std::move(group(sys_render).in_set(set(GameSet::Render))));

  // AudioSet after Render
  schedule.add_set_config(
      configure<AudioSet>().after(set(GameSet::Render)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0], "render");
  EXPECT_EQ(entries[1], "audio");
}

// ============================================================
// Set with run_if condition
// ============================================================

TEST(SystemSetTest, SetRunIfConditionTrue) {
  World world;
  world.insert_resource(Counter{1});
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_movement).in_set(set(PhysicsSet::Movement))));
  schedule.add_system_fn(
      std::move(group(sys_collision).in_set(set(PhysicsSet::Movement))));

  schedule.add_set_config(
      configure(PhysicsSet::Movement)
          .run_if([](Res<Counter> c) { return c->value > 0; })
          .take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  EXPECT_EQ(entries.size(), 2u);
}

TEST(SystemSetTest, SetRunIfConditionFalse) {
  World world;
  world.insert_resource(Counter{0});
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_movement).in_set(set(PhysicsSet::Movement))));
  schedule.add_system_fn(
      std::move(group(sys_collision).in_set(set(PhysicsSet::Movement))));

  schedule.add_set_config(
      configure(PhysicsSet::Movement)
          .run_if([](Res<Counter> c) { return c->value > 0; })
          .take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  EXPECT_EQ(entries.size(), 0u);
}

// ============================================================
// Chained set ordering: A before B before C
// ============================================================

TEST(SystemSetTest, ChainedSetOrdering) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_render).in_set(set(GameSet::Render))));
  schedule.add_system_fn(
      std::move(group(sys_logic).in_set(set(GameSet::Logic))));
  schedule.add_system_fn(
      std::move(group(sys_input).in_set(set(GameSet::Input))));

  // Input -> Logic -> Render
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());
  schedule.add_set_config(
      configure(GameSet::Logic).before(set(GameSet::Render)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0], "input");
  EXPECT_EQ(entries[1], "logic");
  EXPECT_EQ(entries[2], "render");
}

// ============================================================
// System in multiple sets
// ============================================================

TEST(SystemSetTest, SystemInMultipleSets) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  // sys_logic is in both GameSet::Logic and PhysicsSet::Movement
  schedule.add_system_fn(std::move(group(sys_logic)
                                       .in_set(set(GameSet::Logic))
                                       .in_set(set(PhysicsSet::Movement))));
  schedule.add_system_fn(
      std::move(group(sys_input).in_set(set(GameSet::Input))));

  // Input before Logic
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());

  schedule.run(world);

  auto &entries = world.get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0], "input");
  EXPECT_EQ(entries[1], "logic");
}

// ============================================================
// Systems without sets are unaffected
// ============================================================

TEST(SystemSetTest, SystemsWithoutSetsUnaffected) {
  World world;
  world.insert_resource(Log{});
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(sys_movement).in_set(set(PhysicsSet::Movement))));
  // sys without any set
  schedule.add_system_fn([](ResMut<Counter> c) { c->value = 42; });

  schedule.add_set_config(
      configure(PhysicsSet::Movement)
          .run_if([](Res<Counter> c) { return c->value > 0; })
          .take());

  schedule.run(world);

  // Counter system runs (no set, no condition), movement skipped (counter was 0)
  EXPECT_EQ(world.get_resource<Counter>()->value, 42);
  auto &entries = world.get_resource<Log>()->entries;
  EXPECT_EQ(entries.size(), 0u);
}
