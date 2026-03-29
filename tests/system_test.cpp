#include <gtest/gtest.h>

#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

// ============================================================
// Components / Resources
// ============================================================

struct Flag {
  using T = Resource;
  bool value = false;
};

struct Counter {
  using T = Resource;
  int value = 0;
};

} // namespace

// ============================================================
// System tests (mirrors system_test.rs)
// ============================================================

TEST(SystemTest, ResourceManagement) {
  World world;

  EXPECT_FALSE(world.has_resource<Flag>());
  world.insert_resource(Flag{false});
  EXPECT_TRUE(world.has_resource<Flag>());
  world.remove_resource<Flag>();
  EXPECT_FALSE(world.has_resource<Flag>());
}

TEST(SystemTest, MakeSystemExecution) {
  World world;
  world.insert_resource(Counter{0});

  auto sys = make_system([](ResMut<Counter> counter) { counter->value += 1; });
  sys->run(world);
  sys->run(world);
  sys->run(world);

  EXPECT_EQ(world.get_resource<Counter>()->value, 3);
}

TEST(SystemTest, ScheduleWithResources) {
  World world;
  Schedule schedule;

  schedule.add_system_fn([](ResMut<Flag> flag, ResMut<Counter> counter) {
    if (flag->value) {
      counter->value += 1;
    }
  });

  world.insert_resource(Flag{false});
  world.insert_resource(Counter{0});

  schedule.run(world);
  EXPECT_EQ(world.get_resource<Counter>()->value, 0);

  world.get_resource<Flag>()->value = true;
  schedule.run(world);
  EXPECT_EQ(world.get_resource<Counter>()->value, 1);
}

TEST(SystemTest, MultipleSystemsInSchedule) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;

  schedule.add_system_fn([](ResMut<Counter> counter) { counter->value += 1; });

  schedule.add_system_fn([](ResMut<Counter> counter) { counter->value += 10; });

  schedule.run(world);
  EXPECT_EQ(world.get_resource<Counter>()->value, 11);
}
