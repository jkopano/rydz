#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace bench_schedule {

// In the Rust benchmark, ETS uses a monolithic struct per archetype.
// In our component-based ECS, we model this with separate components
// and queries that match the same logical operation.

struct CompA {
  float value;
};

struct CompB {
  float value;
};

struct CompC {
  float value;
};

struct CompD {
  float value;
};

struct CompE {
  float value;
};

// Tag components to distinguish archetype groups
struct TagAB {};
struct TagABC {};
struct TagABCD {};
struct TagABCE {};

static void populate_world(World &world) {
  // 10,000 entities with A, B (AB group)
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, TagAB{});
    world.insert_component(e, CompA{0.0f});
    world.insert_component(e, CompB{0.0f});
  }

  // 10,000 entities with A, B, C (ABC group)
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, TagABC{});
    world.insert_component(e, CompA{0.0f});
    world.insert_component(e, CompB{0.0f});
    world.insert_component(e, CompC{0.0f});
  }

  // 10,000 entities with A, B, C, D (ABCD group)
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, TagABCD{});
    world.insert_component(e, CompA{0.0f});
    world.insert_component(e, CompB{0.0f});
    world.insert_component(e, CompC{0.0f});
    world.insert_component(e, CompD{0.0f});
  }

  // 10,000 entities with A, B, C, E (ABCE group)
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, TagABCE{});
    world.insert_component(e, CompA{0.0f});
    world.insert_component(e, CompB{0.0f});
    world.insert_component(e, CompC{0.0f});
    world.insert_component(e, CompE{0.0f});
  }
}

static void BM_Schedule(benchmark::State &state) {
  World world;
  populate_world(world);

  for (auto _ : state) {
    // System 1: swap A and B on all entities that have both (all 40,000)
    {
      Query<Mut<CompA>, Mut<CompB>> query(world);
      query.each([](CompA *a, CompB *b) { std::swap(a->value, b->value); });
    }

    // System 2: swap C and D on ABCD entities (10,000)
    {
      Query<Mut<CompC>, Mut<CompD>> query(world);
      query.each([](CompC *c, CompD *d) { std::swap(c->value, d->value); });
    }

    // System 3: swap C and E on ABCE entities (10,000)
    {
      Query<Mut<CompC>, Mut<CompE>> query(world);
      query.each([](CompC *c, CompE *e) { std::swap(c->value, e->value); });
    }
  }
}
BENCHMARK(BM_Schedule)->MinTime(3);

} // namespace bench_schedule
