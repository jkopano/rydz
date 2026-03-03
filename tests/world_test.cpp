#include <gtest/gtest.h>

#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// World tests (mirrors world_test.rs)
// ============================================================

struct MyResource {
  int value;
  bool operator==(const MyResource &o) const { return value == o.value; }
};

TEST(WorldTest, ResourceInsertGetRemove) {
  World world;

  EXPECT_FALSE(world.contains_resource<MyResource>());

  world.insert_resource(MyResource{42});
  EXPECT_TRUE(world.contains_resource<MyResource>());

  auto *res = world.get_resource<MyResource>();
  ASSERT_NE(res, nullptr);
  EXPECT_EQ(res->value, 42);

  world.remove_resource<MyResource>();
  EXPECT_FALSE(world.contains_resource<MyResource>());
}
