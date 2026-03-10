#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Components
// ============================================================

struct Position {
  float x, y, z;
};

struct Velocity {
  float x, y, z;
};

// ============================================================
// Benchmarks (mirrors ecs_bench.rs)
// ============================================================

static void BM_EntityCreation10000(benchmark::State &state) {
  for (auto _ : state) {
    World world;
    for (int i = 0; i < 10000; i++) {
      auto e = world.spawn();
      world.insert_component(e, Position{0.0f, 0.0f, 0.0f});
    }
    benchmark::DoNotOptimize(world);
  }
}
BENCHMARK(BM_EntityCreation10000);

static void BM_QueryIterationReadWrite10000(benchmark::State &state) {
  World world;
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, Position{static_cast<float>(i),
                                       static_cast<float>(i),
                                       static_cast<float>(i)});
    world.insert_component(e, Velocity{1.0f, 1.0f, 1.0f});
  }

  for (auto _ : state) {
    Query<Mut<Position>, Velocity> query(world);
    query.each([](Position *pos, Velocity const *vel) {
      pos->x += vel->x;
      pos->y += vel->y;
      pos->z += vel->z;
    });
  }
}
BENCHMARK(BM_QueryIterationReadWrite10000);

static void BM_QueryIterationReadOnly10000(benchmark::State &state) {
  World world;
  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, Position{static_cast<float>(i),
                                       static_cast<float>(i),
                                       static_cast<float>(i)});
    world.insert_component(e, Velocity{1.0f, 1.0f, 1.0f});
  }

  for (auto _ : state) {
    Query<Position, Velocity> query(world);
    query.each([](Position const *pos, Velocity const *vel) {
      benchmark::DoNotOptimize(pos->x + vel->x);
    });
  }
}
BENCHMARK(BM_QueryIterationReadOnly10000);

// Main is in benches/main.cpp
