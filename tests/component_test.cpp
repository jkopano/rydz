#include <gtest/gtest.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

// ============================================================
// Components used in tests
// ============================================================

struct Position {
  int x, y;
  bool operator==(const Position &o) const { return x == o.x && y == o.y; }
};

struct Velocity {
  int x, y;
  bool operator==(const Velocity &o) const { return x == o.x && y == o.y; }
};

struct CompTag {
  const char *label;
  bool operator==(const CompTag &o) const {
    return std::string_view(label) == std::string_view(o.label);
  }
};

struct TestResource {
  using T = Resource;
  int value;
  bool operator==(const TestResource &o) const { return value == o.value; }
};

} // namespace

// ============================================================
// Component tests (mirrors component_test.rs)
// ============================================================

TEST(ComponentTest, InsertAndGet) {
  World world;
  auto entity = world.spawn();

  world.insert_component(entity, Position{10, 20});

  auto *pos = world.get_component<Position>(entity);
  ASSERT_NE(pos, nullptr);
  EXPECT_EQ(*pos, (Position{10, 20}));

  // Modify via pointer
  pos->x = 30;

  auto *pos2 = world.get_component<Position>(entity);
  ASSERT_NE(pos2, nullptr);
  EXPECT_EQ(*pos2, (Position{30, 20}));
}

TEST(ComponentTest, RemoveComponent) {
  World world;
  auto entity = world.spawn();

  world.insert_component(entity, Position{1, 2});
  EXPECT_NE(world.get_component<Position>(entity), nullptr);

  world.remove_component<Position>(entity);
  EXPECT_EQ(world.get_component<Position>(entity), nullptr);
}

TEST(ComponentTest, MultipleComponents) {
  World world;
  auto entity = world.spawn();

  world.insert_component(entity, Position{1, 2});
  world.insert_component(entity, Velocity{3, 4});

  EXPECT_NE(world.get_component<Position>(entity), nullptr);
  EXPECT_NE(world.get_component<Velocity>(entity), nullptr);
}

TEST(ComponentTest, Query) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn(); // no Position

  world.insert_component(e1, Position{1, 2});
  world.insert_component(e2, Position{3, 4});

  int count = 0;
  Query<Position> query(world, Tick{0}, world.read_change_tick());
  query.each([&](const Position *pos) {
    count++;
    EXPECT_NE(pos, nullptr);
  });

  EXPECT_EQ(count, 2);
}

TEST(ComponentTest, HasComponent) {
  World world;
  auto entity = world.spawn();

  EXPECT_FALSE(world.has_component<Position>(entity));
  world.insert_component(entity, Position{0, 0});
  EXPECT_TRUE(world.has_component<Position>(entity));
}

TEST(ComponentTest, HashMapStorage) {
  World world;
  world.ensure_storage_exist<CompTag>();

  auto e1 = world.spawn();
  auto e2 = world.spawn();

  auto &storage = world.ensure_storage_exist<CompTag>();
  storage.insert(e1, CompTag{"player"}, world.read_change_tick());
  storage.insert(e2, CompTag{"enemy"}, world.read_change_tick());

  EXPECT_NE(storage.get(e1), nullptr);
  EXPECT_EQ(*storage.get(e1), (CompTag{"player"}));
  EXPECT_NE(storage.get(e2), nullptr);
  EXPECT_EQ(*storage.get(e2), (CompTag{"enemy"}));

  int count = 0;
  storage.for_each([&](Entity, const CompTag &) { count++; });
  EXPECT_EQ(count, 2);

  storage.remove(e1);
  EXPECT_EQ(storage.get(e1), nullptr);
  EXPECT_NE(storage.get(e2), nullptr);
  EXPECT_EQ(*storage.get(e2), (CompTag{"enemy"}));
}

TEST(ComponentTest, HashMapStorageWithDespawn) {
  World world;
  world.ensure_storage_exist<CompTag>();

  auto e1 = world.spawn();

  auto &storage = world.ensure_storage_exist<CompTag>();
  storage.insert(e1, CompTag{"temp"}, world.read_change_tick());
  world.insert_component(e1, Position{1, 2});

  world.despawn(e1);

  // HashMapStorage removal happens through IStorage::remove in despawn
  EXPECT_EQ(storage.get(e1), nullptr);
  EXPECT_EQ(world.get_component<Position>(e1), nullptr);
}

TEST(ComponentTest, Query2) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{1, 2});
  world.insert_component(e1, Velocity{10, 20});

  world.insert_component(e2, Position{3, 4});
  // e2 has no Velocity

  world.insert_component(e3, Velocity{30, 40});
  // e3 has no Position

  int count = 0;
  Query<Position, Velocity> query(world, Tick{0}, world.read_change_tick());
  query.each([&](const Position *pos, const Velocity *vel) {
    count++;
    EXPECT_EQ(*pos, (Position{1, 2}));
    EXPECT_EQ(*vel, (Velocity{10, 20}));
  });

  EXPECT_EQ(count, 1);
}

TEST(ComponentTest, ResourceContains) {
  World world;

  EXPECT_FALSE(world.has_resource<TestResource>());
  world.insert_resource(TestResource{42});
  EXPECT_TRUE(world.has_resource<TestResource>());
  world.remove_resource<TestResource>();
  EXPECT_FALSE(world.has_resource<TestResource>());
}
