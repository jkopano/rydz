#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Components (mirrors simple_iter.rs using basic float3 types)
// ============================================================

struct Transform {
  float m[16];
};

struct Position {
  float x, y, z;
};

struct Rotation {
  float x, y, z;
};

struct Velocity {
  float x, y, z;
};

// ============================================================
// Benchmark (mirrors simple_iter.rs)
// ============================================================

static void BM_SimpleIter(benchmark::State &state) {
  World world;

  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    float f = static_cast<float>(i);
    world.insert_component(e, Transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    world.insert_component(e, Position{f, f, f});
    world.insert_component(e, Rotation{f, f, f});
    world.insert_component(e, Velocity{f, f, f});
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
BENCHMARK(BM_SimpleIter)->MinTime(3);

// Main is in benches/main.cpp
