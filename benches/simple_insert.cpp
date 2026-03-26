#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace bench_simple_insert {

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

static void BM_SimpleInsert(benchmark::State &state) {
  for (auto _ : state) {
    World world;
    for (int i = 0; i < 10000; i++) {
      auto e = world.spawn();
      world.insert_component(e,
                             Transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
      world.insert_component(e, Position{1.0f, 0.0f, 0.0f});
      world.insert_component(e, Rotation{1.0f, 0.0f, 0.0f});
      world.insert_component(e, Velocity{1.0f, 0.0f, 0.0f});
    }
    benchmark::DoNotOptimize(world);
  }
}
BENCHMARK(BM_SimpleInsert)->MinTime(3);

} // namespace bench_simple_insert
