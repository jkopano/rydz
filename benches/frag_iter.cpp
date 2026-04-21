#include <benchmark/benchmark.h>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace bench_frag_iter {

struct Data {
  float value;
};

struct A { float v; };
struct B { float v; };
struct C { float v; };
struct D { float v; };
struct E { float v; };
struct F { float v; };
struct G { float v; };
struct H { float v; };
struct I { float v; };
struct J { float v; };
struct K { float v; };
struct L { float v; };
struct M { float v; };
struct N { float v; };
struct O { float v; };
struct P { float v; };
struct Q { float v; };
struct R { float v; };
struct S { float v; };
struct T { float v; };
struct U { float v; };
struct V { float v; };
struct W { float v; };
struct X { float v; };
struct Y { float v; };
struct Z { float v; };

template <typename Tag>
static void populate(World &world, int n) {
  for (int i = 0; i < n; i++) {
    auto e = world.spawn();
    world.insert_component(e, Tag{1.0f});
    world.insert_component(e, Data{1.0f});
  }
}

static void populate_world(World &world) {
  const int n = 20;
  populate<A>(world, n);
  populate<B>(world, n);
  populate<C>(world, n);
  populate<D>(world, n);
  populate<E>(world, n);
  populate<F>(world, n);
  populate<G>(world, n);
  populate<H>(world, n);
  populate<I>(world, n);
  populate<J>(world, n);
  populate<K>(world, n);
  populate<L>(world, n);
  populate<M>(world, n);
  populate<N>(world, n);
  populate<O>(world, n);
  populate<P>(world, n);
  populate<Q>(world, n);
  populate<R>(world, n);
  populate<S>(world, n);
  populate<T>(world, n);
  populate<U>(world, n);
  populate<V>(world, n);
  populate<W>(world, n);
  populate<X>(world, n);
  populate<Y>(world, n);
  populate<Z>(world, n);
}

static void BM_FragIter(benchmark::State &state) {
  World world;
  populate_world(world);

  for (auto _ : state) {
    Query<Mut<Data>> query(world);
    query.each([](Data *data) { data->value *= 2.0f; });
  }
}
BENCHMARK(BM_FragIter)->MinTime(3);

} // namespace bench_frag_iter
