#include <gtest/gtest.h>

#include "rydz_ecs/app.hpp"
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
  schedule.add_system_fn(PhysicsSet::Collision, group(sys_collision));
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_movement));

  // Movement before Collision
  schedule.add_set_config(
      configure(PhysicsSet::Movement).before(set(PhysicsSet::Collision)).take());
  schedule.add_set_config(configure(PhysicsSet::Collision).take());

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
  schedule.add_system_fn(GameSet::Render, group(sys_render));
  schedule.add_system_fn(GameSet::Logic, group(sys_logic));

  // Render after Logic
  schedule.add_set_config(
      configure(GameSet::Render).after(set(GameSet::Logic)).take());
  schedule.add_set_config(configure(GameSet::Logic).take());

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
  schedule.add_system_fn(GameSet::Input, group(sys_input));
  schedule.add_system_fn(GameSet::Input, group(sys_movement));

  // One system in Logic set
  schedule.add_system_fn(GameSet::Logic, group(sys_logic));

  // Input before Logic
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());
  schedule.add_set_config(configure(GameSet::Logic).take());

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
  schedule.add_system_fn(AudioSet{}, group(sys_audio));
  schedule.add_system_fn(GameSet::Render, group(sys_render));

  // AudioSet after Render
  schedule.add_set_config(
      configure<AudioSet>().after(set(GameSet::Render)).take());
  schedule.add_set_config(configure(GameSet::Render).take());

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
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_movement));
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_collision));

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
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_movement));
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_collision));

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
  schedule.add_system_fn(GameSet::Render, group(sys_render));
  schedule.add_system_fn(GameSet::Logic, group(sys_logic));
  schedule.add_system_fn(GameSet::Input, group(sys_input));

  // Input -> Logic -> Render
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());
  schedule.add_set_config(
      configure(GameSet::Logic).before(set(GameSet::Render)).take());
  schedule.add_set_config(configure(GameSet::Render).take());

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
  schedule.add_system_fn(sets(set(GameSet::Logic), set(PhysicsSet::Movement)),
                         group(sys_logic));
  schedule.add_system_fn(GameSet::Input, group(sys_input));

  // Input before Logic
  schedule.add_set_config(
      configure(GameSet::Input).before(set(GameSet::Logic)).take());
  schedule.add_set_config(configure(GameSet::Logic).take());
  schedule.add_set_config(configure(PhysicsSet::Movement).take());

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
  schedule.add_system_fn(PhysicsSet::Movement, group(sys_movement));
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

TEST(SystemSetTest, MissingSetConfigurationThrowsOnRun) {
  World world;
  world.insert_resource(Log{});

  Schedule schedule;
  schedule.add_system_fn(GameSet::Input, group(sys_input));

  EXPECT_THROW(schedule.run(world), std::runtime_error);
}

TEST(SystemSetTest, AppRoutesSetSystemsToConfiguredSchedule) {
  App app;
  app.insert_resource(Log{});

  app.configure_set(ScheduleLabel::Startup, configure(GameSet::Input))
      .add_systems(GameSet::Input, group(sys_input));

  app.startup();

  auto &entries = app.world().get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], "input");
}

TEST(SystemSetTest, AppAllowsConfigureAfterAddUntilRun) {
  App app;
  app.insert_resource(Log{});

  app.add_systems(GameSet::Input, group(sys_input));
  app.configure_set(ScheduleLabel::Startup, configure(GameSet::Input));

  app.startup();

  auto &entries = app.world().get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], "input");
}

TEST(SystemSetTest, AppMissingSetConfigurationThrowsOnRun) {
  App app;
  app.insert_resource(Log{});

  app.add_systems(GameSet::Input, group(sys_input));

  EXPECT_THROW(app.startup(), std::runtime_error);
}

TEST(SystemSetTest, AppRejectsConflictingSetSchedules) {
  App app;

  app.configure_set(ScheduleLabel::Startup, configure(GameSet::Input));
  EXPECT_THROW(
      app.configure_set(ScheduleLabel::Update, configure(GameSet::Input)),
      std::runtime_error);
}

TEST(SystemSetTest, ConfigureChainOrdersSetsSequentially) {
  App app;
  app.insert_resource(Log{});

  app.configure_set(
         ScheduleLabel::Startup,
         configure(GameSet::Input, GameSet::Logic, GameSet::Render).chain())
      .add_systems(GameSet::Render, group(sys_render))
      .add_systems(GameSet::Logic, group(sys_logic))
      .add_systems(GameSet::Input, group(sys_input));

  app.startup();

  auto &entries = app.world().get_resource<Log>()->entries;
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0], "input");
  EXPECT_EQ(entries[1], "logic");
  EXPECT_EQ(entries[2], "render");
}
