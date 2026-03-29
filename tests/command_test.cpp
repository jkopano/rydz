#include <gtest/gtest.h>

#include "rydz_ecs/command.hpp"
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

struct Name {
  const char *label;
  bool operator==(const Name &o) const {
    return std::string_view(label) == std::string_view(o.label);
  }
};

struct Score {
  int v;
  bool operator==(const Score &o) const { return v == o.v; }
};

struct Value {
  using T = Resource;
  int v;
  bool operator==(const Value &o) const { return v == o.v; }
};

} // namespace

// ============================================================
// Command tests (mirrors command_test.rs / commands_test.rs)
// ============================================================

TEST(CommandTest, SpawnWithComponents) {
  World world;
  world.insert_resource(CommandQueues{});

  {
    auto *queues = world.get_resource<CommandQueues>();
    Cmd cmd(queues, &world.entities);

    cmd.spawn(Health{100}, Name{"Hero"});
  }

  // Apply commands
  auto *queues = world.get_resource<CommandQueues>();
  queues->apply(world);

  // Verify
  int health_count = 0;
  auto *storage = world.get_storage<Health>();
  if (storage) {
    storage->for_each([&](Entity, const Health &h) {
      EXPECT_EQ(h.value, 100);
      health_count++;
    });
  }
  EXPECT_EQ(health_count, 1);

  int name_count = 0;
  auto *name_storage = world.get_storage<Name>();
  if (name_storage) {
    name_storage->for_each([&](Entity, const Name &n) {
      EXPECT_EQ(n, (Name{"Hero"}));
      name_count++;
    });
  }
  EXPECT_EQ(name_count, 1);
}

TEST(CommandTest, SpawnMany) {
  World world;
  world.insert_resource(CommandQueues{});

  {
    auto *queues = world.get_resource<CommandQueues>();
    Cmd cmd(queues, &world.entities);

    for (int i = 0; i < 5; i++) {
      cmd.spawn(Score{i});
    }
  }

  auto *queues = world.get_resource<CommandQueues>();
  queues->apply(world);

  int count = 0;
  auto *storage = world.get_storage<Score>();
  if (storage) {
    storage->for_each([&](Entity, const Score &) { count++; });
  }
  EXPECT_EQ(count, 5);
}

TEST(CommandTest, DespawnCommand) {
  World world;
  world.insert_resource(CommandQueues{});

  auto entity = world.spawn();
  world.insert_component(entity, Health{100});
  EXPECT_NE(world.get_component<Health>(entity), nullptr);

  {
    auto *queues = world.get_resource<CommandQueues>();
    Cmd cmd(queues, &world.entities);
    cmd.despawn(entity);
  }

  auto *queues = world.get_resource<CommandQueues>();
  queues->apply(world);

  EXPECT_EQ(world.get_component<Health>(entity), nullptr);
}

TEST(CommandTest, InsertResourceCommand) {
  World world;
  world.insert_resource(CommandQueues{});

  {
    auto *queues = world.get_resource<CommandQueues>();
    Cmd cmd(queues, &world.entities);
    cmd.insert_resource(Value{42});
  }

  auto *queues = world.get_resource<CommandQueues>();
  queues->apply(world);

  auto *val = world.get_resource<Value>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->v, 42);
}

TEST(CommandTest, SpawnEmptyThenInsert) {
  World world;
  world.insert_resource(CommandQueues{});

  {
    auto *queues = world.get_resource<CommandQueues>();
    Cmd cmd(queues, &world.entities);
    auto ec = cmd.spawn_empty();
    ec.insert(Health{50});
    ec.insert(Name{"NPC"});
  }

  auto *queues = world.get_resource<CommandQueues>();
  queues->apply(world);

  int count = 0;
  auto *storage = world.get_storage<Health>();
  if (storage) {
    storage->for_each([&](Entity, const Health &h) {
      EXPECT_EQ(h.value, 50);
      count++;
    });
  }
  EXPECT_EQ(count, 1);
}
