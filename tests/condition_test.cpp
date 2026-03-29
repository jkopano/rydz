#include <gtest/gtest.h>

#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

// ============================================================
// Condition tests (mirrors condition_test.rs)
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

TEST(ConditionTest, RunOnce) {
  World world;
  int run_count = 0;

  auto system = make_system_run_if([&]() { run_count++; },
                                   [ran = false]() mutable -> bool {
                                     if (!ran) {
                                       ran = true;
                                       return true;
                                     }
                                     return false;
                                   });

  system->run(world);
  EXPECT_EQ(run_count, 1);

  system->run(world);
  EXPECT_EQ(run_count, 1); // didn't run again

  system->run(world);
  EXPECT_EQ(run_count, 1); // still didn't
}

TEST(ConditionTest, RunOnceBuiltin) {
  World world;
  int run_count = 0;

  auto cond = run_once();
  auto sys = make_system([&]() { run_count++; });
  auto conditioned =
      std::make_unique<ConditionedSystem>(std::move(sys), std::move(cond));

  conditioned->run(world);
  EXPECT_EQ(run_count, 1);

  conditioned->run(world);
  EXPECT_EQ(run_count, 1);
}

TEST(ConditionTest, MakeConditionTrue) {
  World world;
  bool ran = false;

  auto cond = make_condition([]() -> bool { return true; });
  auto sys = make_system([&]() { ran = true; });
  auto conditioned =
      std::make_unique<ConditionedSystem>(std::move(sys), std::move(cond));

  conditioned->run(world);
  EXPECT_TRUE(ran);
}

TEST(ConditionTest, MakeConditionFalse) {
  World world;
  bool ran = false;

  auto cond = make_condition([]() -> bool { return false; });
  auto sys = make_system([&]() { ran = true; });
  auto conditioned =
      std::make_unique<ConditionedSystem>(std::move(sys), std::move(cond));

  conditioned->run(world);
  EXPECT_FALSE(ran);
}

TEST(ConditionTest, ConditionWithResource) {
  World world;
  world.insert_resource(Flag{true});
  int run_count = 0;

  auto cond =
      make_condition([](Res<Flag> flag) -> bool { return flag->value; });
  auto sys = make_system([&]() { run_count++; });
  auto conditioned =
      std::make_unique<ConditionedSystem>(std::move(sys), std::move(cond));

  conditioned->run(world);
  EXPECT_EQ(run_count, 1);

  // Set flag to false
  world.get_resource<Flag>()->value = false;

  conditioned->run(world);
  EXPECT_EQ(run_count, 1); // didn't run
}

// ============================================================
// into_system().run_if() builder tests
// ============================================================

TEST(ConditionTest, IntoSystemRunIfLambda) {
  World world;
  int run_count = 0;

  auto desc =
      group([&]() { run_count++; }).run_if([]() -> bool { return false; });

  auto system = desc.build();

  system->run(world);
  EXPECT_EQ(run_count, 0); // condition is false, shouldn't run
}

TEST(ConditionTest, IntoSystemRunIfTrue) {
  World world;
  int run_count = 0;

  auto system = group([&]() {
                  run_count++;
                }).run_if([]() -> bool {
                    return true;
                  }).build();

  system->run(world);
  EXPECT_EQ(run_count, 1);

  system->run(world);
  EXPECT_EQ(run_count, 2);
}

TEST(ConditionTest, IntoSystemRunOnce) {
  World world;
  int run_count = 0;

  auto system = group([&]() { run_count++; }).run_if(run_once()).build();

  system->run(world);
  EXPECT_EQ(run_count, 1);

  system->run(world);
  EXPECT_EQ(run_count, 1); // didn't run again
}

TEST(ConditionTest, IntoSystemNoCondition) {
  World world;
  int run_count = 0;

  auto system = group([&]() { run_count++; }).build();

  system->run(world);
  EXPECT_EQ(run_count, 1);

  system->run(world);
  EXPECT_EQ(run_count, 2);
}
