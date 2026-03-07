#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Components (mirrors sparse_bench.rs)
// ============================================================

struct Position {
  float x, y, z;
};

struct SparseData {
  int value;
};

struct MapData {
  int value;
};

struct DenseData {
  int value;
};

// ============================================================
// Benchmarks (mirrors sparse_bench.rs)
// ============================================================

static void BM_VecStorageIteration10Pct(benchmark::State &state) {
  World world;
  const int total = 10000;
  const int sparsity = 10;

  for (int i = 0; i < total; i++) {
    auto e = world.spawn();
    world.insert_component(e, Position{0.0f, 0.0f, 0.0f});
    if (i % sparsity == 0) {
      world.insert_component(e, DenseData{i});
    }
  }

  for (auto _ : state) {
    Query<Position, DenseData> query(world);
    query.for_each([](const Position *, const DenseData *val) {
      benchmark::DoNotOptimize(val->value);
    });
  }
}
BENCHMARK(BM_VecStorageIteration10Pct);

static void BM_HashMapStorageIteration10Pct(benchmark::State &state) {
  World world;
  const int total = 10000;
  const int sparsity = 10;

  auto &map_storage = world.ensure_hashmap_storage<MapData>();

  for (int i = 0; i < total; i++) {
    auto e = world.spawn();
    world.insert_component(e, Position{0.0f, 0.0f, 0.0f});
    if (i % sparsity == 0) {
      map_storage.insert(e, MapData{i}, world.read_change_tick());
    }
  }

  for (auto _ : state) {
    // For HashMapStorage we iterate manually since Query uses VecStorage
    auto *pos_storage = world.get_storage<Position>();
    if (!pos_storage)
      continue;

    map_storage.for_each([&](Entity e, const MapData &val) {
      auto *pos = pos_storage->get(e);
      if (pos) {
        benchmark::DoNotOptimize(val.value);
      }
    });
  }
}
BENCHMARK(BM_HashMapStorageIteration10Pct);

// Main is in benches/main.cpp
