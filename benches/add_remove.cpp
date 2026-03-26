#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace bench_add_remove {

struct A {
  float value;
};

struct B {
  float value;
};

static void BM_AddRemoveComponent(benchmark::State &state) {
  World world;
  std::vector<Entity> entities;
  entities.reserve(10000);

  for (int i = 0; i < 10000; i++) {
    auto e = world.spawn();
    world.insert_component(e, A{0.0f});
    entities.push_back(e);
  }

  for (auto _ : state) {
    for (auto e : entities) {
      world.insert_component(e, B{0.0f});
    }
    for (auto e : entities) {
      world.remove_component<B>(e);
    }
  }
}
BENCHMARK(BM_AddRemoveComponent)->MinTime(3);

} // namespace bench_add_remove
