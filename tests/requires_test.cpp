#include <gtest/gtest.h>
#include "rydz_ecs/rydz_ecs.hpp"

using namespace ecs;

struct CompA {
    using T = Component;
    int val = 1;
};

struct CompB {
    using T = Component;
    int val = 2;
};

struct CompWithReq {
    using T = Component;
    using Required = Requires<CompA, CompB>;
    int val = 3;
};

TEST(RequiresTest, AutomaticallyInsertsRequiredComponents) {
    World world;
    Entity e = world.spawn();

    world.insert_component(e, CompWithReq{});

    // Check that CompWithReq is there
    EXPECT_TRUE(world.has_component<CompWithReq>(e));

    // Check that CompA and CompB were automatically inserted
    EXPECT_TRUE(world.has_component<CompA>(e));
    EXPECT_TRUE(world.has_component<CompB>(e));

    // Verify values are default constructed
    EXPECT_EQ(world.get_component<CompA>(e)->val, 1);
    EXPECT_EQ(world.get_component<CompB>(e)->val, 2);
}

struct CompWithRecursiveReq {
    using T = Component;
    using Required = Requires<CompWithReq>;
};

TEST(RequiresTest, RecursiveRequirements) {
    World world;
    Entity e = world.spawn();

    world.insert_component(e, CompWithRecursiveReq{});

    EXPECT_TRUE(world.has_component<CompWithRecursiveReq>(e));
    EXPECT_TRUE(world.has_component<CompWithReq>(e));
    EXPECT_TRUE(world.has_component<CompA>(e));
    EXPECT_TRUE(world.has_component<CompB>(e));
}
