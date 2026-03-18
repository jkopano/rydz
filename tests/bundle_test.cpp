#include <gtest/gtest.h>

#include "rydz_ecs/bundle.hpp"
#include "rydz_ecs/command.hpp"
#include "rydz_ecs/query.hpp"

using namespace ecs;

// ---- Test components ----

struct Position {
  float x, y;
  bool operator==(const Position &o) const { return x == o.x && y == o.y; }
};

struct Velocity {
  float dx, dy;
  bool operator==(const Velocity &o) const { return dx == o.dx && dy == o.dy; }
};

struct Health {
  int hp;
  bool operator==(const Health &o) const { return hp == o.hp; }
};

struct Name {
  std::string value;
};

struct Marker {};

// ---- Test bundles (no components() needed — auto-decomposed) ----

struct MovingBundle {
  using Type = BundleType;
  Position pos;
  Velocity vel;
};

struct PlayerBundle {
  using Type = BundleType;
  MovingBundle moving;
  Health health;
  Name name;
};

struct TaggedBundle {
  using Type = BundleType;
  Marker marker;
  Position pos;
};

// ============================================================
// Type tag detection
// ============================================================

TEST(BundleTest, BundleConceptDetection) {
  static_assert(Bundle<MovingBundle>);
  static_assert(Bundle<PlayerBundle>);
  static_assert(Bundle<TaggedBundle>);
  static_assert(!Bundle<Position>);
  static_assert(!Bundle<int>);
  static_assert(!Bundle<std::string>);
}

TEST(BundleTest, ComponentConceptDetection) {
  static_assert(Component<Position>);
  static_assert(Component<Velocity>);
  static_assert(Component<Health>);
  static_assert(Component<Marker>);
  static_assert(Component<int>);
  static_assert(!Component<MovingBundle>);
}

TEST(BundleTest, ResourceConceptDetection) {
  static_assert(Resource<CommandQueue>);
  static_assert(!Resource<Position>);
  static_assert(!Resource<MovingBundle>);
}

TEST(BundleTest, SpawnableConceptDetection) {
  static_assert(Spawnable<Position>);
  static_assert(Spawnable<MovingBundle>);
  static_assert(!Spawnable<CommandQueue>); // ResourceType, not spawnable
}

// ============================================================
// insert_bundle — direct World usage
// ============================================================

TEST(BundleTest, InsertPlainComponent) {
  World world;
  Entity e = world.spawn();
  insert_bundle(world, e, Position{1.0f, 2.0f});

  auto *pos = world.get_component<Position>(e);
  ASSERT_NE(pos, nullptr);
  EXPECT_EQ(*pos, (Position{1.0f, 2.0f}));
}

TEST(BundleTest, InsertSimpleBundle) {
  World world;
  Entity e = world.spawn();
  insert_bundle(world, e,
                MovingBundle{.pos = {5.0f, 6.0f}, .vel = {-1.0f, 2.0f}});

  EXPECT_EQ(*world.get_component<Position>(e), (Position{5.0f, 6.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{-1.0f, 2.0f}));
}

TEST(BundleTest, InsertNestedBundle) {
  World world;
  Entity e = world.spawn();

  insert_bundle(world, e,
                PlayerBundle{
                    .moving = {.pos = {10.0f, 20.0f}, .vel = {1.0f, 1.0f}},
                    .health = {100},
                    .name = {"Hero"},
                });

  EXPECT_EQ(*world.get_component<Position>(e), (Position{10.0f, 20.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{1.0f, 1.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 100);
  EXPECT_EQ(world.get_component<Name>(e)->value, "Hero");
}

TEST(BundleTest, InsertMultipleItemsAtOnce) {
  World world;
  Entity e = world.spawn();

  insert_bundle(world, e, Position{1.0f, 2.0f}, Velocity{3.0f, 4.0f},
                Health{50});

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1.0f, 2.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{3.0f, 4.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 50);
}

TEST(BundleTest, InsertBundleAndLooseComponentsTogether) {
  World world;
  Entity e = world.spawn();

  insert_bundle(world, e,
                MovingBundle{.pos = {1.0f, 0.0f}, .vel = {0.0f, 1.0f}},
                Health{75});

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1.0f, 0.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{0.0f, 1.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 75);
}

// ============================================================
// Cmd::spawn with bundles
// ============================================================

TEST(BundleTest, CmdSpawnBundle) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn(MovingBundle{.pos = {1.0f, 2.0f}, .vel = {3.0f, 4.0f}});
  Entity e = ec.id();

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1.0f, 2.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{3.0f, 4.0f}));
}

TEST(BundleTest, CmdSpawnNestedBundle) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn(PlayerBundle{
      .moving = {.pos = {0.0f, 0.0f}, .vel = {5.0f, 5.0f}},
      .health = {200},
      .name = {"Player1"},
  });
  Entity e = ec.id();

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{0.0f, 0.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{5.0f, 5.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 200);
  EXPECT_EQ(world.get_component<Name>(e)->value, "Player1");
}

TEST(BundleTest, CmdSpawnBundleMixedWithComponents) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn(MovingBundle{.pos = {1.0f, 1.0f}, .vel = {2.0f, 2.0f}},
                      Health{42});
  Entity e = ec.id();

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1.0f, 1.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{2.0f, 2.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 42);
}

// ============================================================
// EntityCommands::insert with bundles
// ============================================================

TEST(BundleTest, EntityCommandsInsertBundle) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn_empty();
  Entity e = ec.id();
  ec.insert(MovingBundle{.pos = {7.0f, 8.0f}, .vel = {9.0f, 10.0f}});

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{7.0f, 8.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{9.0f, 10.0f}));
}

TEST(BundleTest, EntityCommandsInsertNestedBundle) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn_empty();
  Entity e = ec.id();
  ec.insert(PlayerBundle{
      .moving = {.pos = {1.0f, 2.0f}, .vel = {3.0f, 4.0f}},
      .health = {99},
      .name = {"NPC"},
  });

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1.0f, 2.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{3.0f, 4.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 99);
  EXPECT_EQ(world.get_component<Name>(e)->value, "NPC");
}

// ============================================================
// Bundle overwrites existing components
// ============================================================

TEST(BundleTest, BundleOverwritesExistingComponents) {
  World world;
  Entity e = world.spawn();
  world.insert_component(e, Position{0.0f, 0.0f});

  insert_bundle(world, e, MovingBundle{.pos = {99.0f, 99.0f}, .vel = {1, 1}});

  EXPECT_EQ(*world.get_component<Position>(e), (Position{99.0f, 99.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{1.0f, 1.0f}));
}

// ============================================================
// Empty marker bundles
// ============================================================

struct EmptyMarkerBundle {
  using Type = BundleType;
  Marker marker;
};

TEST(BundleTest, MarkerBundle) {
  World world;
  Entity e = world.spawn();
  insert_bundle(world, e, EmptyMarkerBundle{});

  EXPECT_TRUE(world.has_component<Marker>(e));
}

// ============================================================
// Query works with bundle-spawned entities
// ============================================================

TEST(BundleTest, QueryMatchesBundleSpawnedEntities) {
  World world;
  Entity e1 = world.spawn();
  insert_bundle(world, e1,
                MovingBundle{.pos = {1.0f, 0.0f}, .vel = {0.0f, 1.0f}});

  Entity e2 = world.spawn();
  insert_bundle(world, e2,
                MovingBundle{.pos = {2.0f, 0.0f}, .vel = {0.0f, 2.0f}});

  // Only give e2 health
  world.insert_component(e2, Health{50});

  Query<Position, Velocity> all_query(world);
  int count = 0;
  all_query.each([&](const Position *, const Velocity *) { count++; });
  EXPECT_EQ(count, 2);

  Query<Position, Health> health_query(world);
  int health_count = 0;
  health_query.each([&](const Position *p, const Health *h) {
    health_count++;
    EXPECT_EQ(*p, (Position{2.0f, 0.0f}));
    EXPECT_EQ(h->hp, 50);
  });
  EXPECT_EQ(health_count, 1);
}

// ============================================================
// Deeply nested bundles (3 levels)
// ============================================================

struct InnerBundle {
  using Type = BundleType;
  Position pos;
};

struct MiddleBundle {
  using Type = BundleType;
  InnerBundle inner;
  Velocity vel;
};

struct OuterBundle {
  using Type = BundleType;
  MiddleBundle middle;
  Health health;
};

TEST(BundleTest, DeeplyNestedBundle) {
  World world;
  Entity e = world.spawn();
  insert_bundle(world, e,
                OuterBundle{
                    .middle =
                        {
                            .inner = {.pos = {42.0f, 43.0f}},
                            .vel = {1.0f, -1.0f},
                        },
                    .health = {999},
                });

  EXPECT_EQ(*world.get_component<Position>(e), (Position{42.0f, 43.0f}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{1.0f, -1.0f}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 999);
}

// ============================================================
// Multiple entities with same bundle
// ============================================================

TEST(BundleTest, SpawnMultipleEntitiesWithSameBundle) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  std::vector<Entity> entities;
  for (int i = 0; i < 100; i++) {
    auto ec = cmd.spawn(MovingBundle{
        .pos = {static_cast<float>(i), 0.0f},
        .vel = {0.0f, static_cast<float>(i)},
    });
    entities.push_back(ec.id());
  }

  queue->apply(world);

  for (size_t i = 0; i < 100; i++) {
    auto *pos = world.get_component<Position>(entities[i]);
    ASSERT_NE(pos, nullptr);
    EXPECT_EQ(pos->x, static_cast<float>(i));
  }
}

// ============================================================
// Chaining insert calls with bundles
// ============================================================

TEST(BundleTest, ChainingInserts) {
  World world;
  world.insert_resource(CommandQueue{});

  auto *queue = world.get_resource<CommandQueue>();
  Cmd cmd(queue, &world.entities);

  auto ec = cmd.spawn_empty();
  Entity e = ec.id();
  ec.insert(MovingBundle{.pos = {1, 2}, .vel = {3, 4}})
      .insert(Health{100})
      .insert(Marker{});

  queue->apply(world);

  EXPECT_EQ(*world.get_component<Position>(e), (Position{1, 2}));
  EXPECT_EQ(*world.get_component<Velocity>(e), (Velocity{3, 4}));
  EXPECT_EQ(world.get_component<Health>(e)->hp, 100);
  EXPECT_TRUE(world.has_component<Marker>(e));
}
