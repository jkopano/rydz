#include <gtest/gtest.h>

#include "rydz_ecs/params.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

#include <string>

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

struct LocalCounter {
  int value = 0;
};

[[maybe_unused]] void valid_free_system(Res<Counter> /*counter*/) {}

} // namespace

static_assert(SystemCallable<decltype(+valid_free_system)>);
static_assert(SystemCallable<decltype([](Res<Counter> /*counter*/) {})>);
static_assert(SystemCallable<decltype([](World & /*world*/) {})>);
static_assert(!SystemCallable<decltype([](int /*not_a_param*/) {})>);
static_assert(!SystemCallable<decltype([](std::string /*not_a_param*/) {})>);
static_assert(!SystemCallable<decltype([](auto /*generic*/) {})>);

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

TEST(SystemTest, LocalPersistsBetweenRuns) {
  World world;
  int seen = 0;

  auto sys = make_system([&](Local<LocalCounter> local) {
    local->value += 1;
    seen = local->value;
  });

  sys->run(world);
  EXPECT_EQ(seen, 1);
  sys->run(world);
  EXPECT_EQ(seen, 2);
  sys->run(world);
  EXPECT_EQ(seen, 3);
}

TEST(SystemTest, LocalIsPerSystemInstance) {
  World world;
  int a_seen = 0;
  int b_seen = 0;

  auto sys_a = make_system([&](Local<LocalCounter> local) {
    local->value += 1;
    a_seen = local->value;
  });
  auto sys_b = make_system([&](Local<LocalCounter> local) {
    local->value += 10;
    b_seen = local->value;
  });

  sys_a->run(world);
  sys_b->run(world);
  sys_a->run(world);
  sys_b->run(world);

  EXPECT_EQ(a_seen, 2);
  EXPECT_EQ(b_seen, 20);
}
