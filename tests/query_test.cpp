#include <algorithm>
#include <gtest/gtest.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

// ============================================================
// Components
// ============================================================

struct Position {
  int x, y;
  bool operator==(const Position &o) const { return x == o.x && y == o.y; }
};

struct Velocity {
  int x, y;
  bool operator==(const Velocity &o) const { return x == o.x && y == o.y; }
};

struct Tag {};

} // namespace

// ============================================================
// Query tests (mirrors query_test.rs)
// ============================================================

TEST(QueryTest, BasicReadQuery) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();

  world.insert_component(e1, Position{10, 20});
  world.insert_component(e2, Position{30, 40});

  int count = 0;
  Query<Position> query(world);
  query.each([&](const Position *pos) {
    count++;
    EXPECT_NE(pos, nullptr);
  });
  EXPECT_EQ(count, 2);
}

TEST(QueryTest, MutQuery) {
  World world;
  auto e1 = world.spawn();

  world.insert_component(e1, Position{10, 20});

  // Mutate
  {
    Query<Mut<Position>> query(world);
    query.each([&](Position *pos) {
      pos->x += 5;
      pos->y += 5;
    });
  }

  // Verify
  auto *pos = world.get_component<Position>(e1);
  ASSERT_NE(pos, nullptr);
  EXPECT_EQ(*pos, (Position{15, 25}));
}

TEST(QueryTest, TupleQuery) {
  World world;
  auto e1 = world.spawn();

  world.insert_component(e1, Position{10, 20});
  world.insert_component(e1, Velocity{1, 2});

  bool found = false;
  Query<Position, Velocity> query(world);
  query.each([&](const Position *pos, const Velocity *vel) {
    EXPECT_EQ(*pos, (Position{10, 20}));
    EXPECT_EQ(*vel, (Velocity{1, 2}));
    found = true;
  });
  EXPECT_TRUE(found);
}

TEST(QueryTest, WithFilter) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{0, 0});
  world.insert_component(e1, Tag{});

  world.insert_component(e2, Position{10, 10});

  world.insert_component(e3, Position{20, 20});
  world.insert_component(e3, Velocity{1, 1});

  // With<Tag>
  std::vector<int> results;
  Query<Position, Filters<With<Tag>>> query(world);
  query.each([&](const Position *pos) { results.push_back(pos->x); });
  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], 0);
}

TEST(QueryTest, WithoutFilter) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{0, 0});
  world.insert_component(e1, Tag{});

  world.insert_component(e2, Position{10, 10});

  world.insert_component(e3, Position{20, 20});
  world.insert_component(e3, Velocity{1, 1});

  // Without<Tag>
  std::vector<int> results;
  Query<Position, Filters<Without<Tag>>> query(world);
  query.each([&](const Position *pos) { results.push_back(pos->x); });
  std::sort(results.begin(), results.end());
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0], 10);
  EXPECT_EQ(results[1], 20);
}

TEST(QueryTest, OrFilter) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{0, 0});
  world.insert_component(e1, Tag{});

  world.insert_component(e2, Position{10, 10});

  world.insert_component(e3, Position{20, 20});
  world.insert_component(e3, Velocity{1, 1});

  // Or<With<Tag>, With<Velocity>>
  std::vector<int> results;
  Query<Position, Filters<Or<With<Tag>, With<Velocity>>>> query(world);
  query.each([&](const Position *pos) { results.push_back(pos->x); });
  std::sort(results.begin(), results.end());
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0], 0);
  EXPECT_EQ(results[1], 20);
}

TEST(QueryTest, AndFilter) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{0, 0});
  world.insert_component(e1, Tag{});

  world.insert_component(e2, Position{10, 10});

  world.insert_component(e3, Position{20, 20});
  world.insert_component(e3, Velocity{1, 1});

  // Without<Tag> AND With<Velocity>
  std::vector<int> results;
  Query<Position, Filters<Without<Tag>, With<Velocity>>> query(world);
  query.each([&](const Position *pos) { results.push_back(pos->x); });
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], 20);
}

TEST(QueryTest, OptionalComponent) {
  World world;
  auto e1 = world.spawn();
  auto e2 = world.spawn();
  auto e3 = world.spawn();

  world.insert_component(e1, Position{1, 2});
  world.insert_component(e1, Velocity{10, 20});

  world.insert_component(e2, Position{3, 4});
  // e2 has no Velocity

  world.insert_component(e3, Position{5, 6});
  world.insert_component(e3, Velocity{50, 60});

  // Opt<Velocity> always matches, returns nullptr when absent
  std::vector<std::pair<int, bool>> results;
  Query<Position, Opt<Velocity>> query(world);
  query.each([&](const Position *pos, const Velocity *vel) {
    results.push_back({pos->x, vel != nullptr});
  });

  std::sort(results.begin(), results.end());
  ASSERT_EQ(results.size(), 3u);
  EXPECT_EQ(results[0].first, 1);
  EXPECT_TRUE(results[0].second); // e1 has Velocity
  EXPECT_EQ(results[1].first, 3);
  EXPECT_FALSE(results[1].second); // e2 has no Velocity
  EXPECT_EQ(results[2].first, 5);
  EXPECT_TRUE(results[2].second); // e3 has Velocity
}
