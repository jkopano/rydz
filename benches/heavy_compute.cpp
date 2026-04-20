#include <benchmark/benchmark.h>

#include "math.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace bench_heavy_compute {

struct Transform {
  math::Mat4 matrix;
};

struct Position {
  math::Vec3 value;
};

struct Rotation {
  math::Vec3 value;
};

struct Velocity {
  math::Vec3 value;
};

static void populate_world(World &world) {
  // Create rotation matrix around X axis (1.2 radians), matching Rust's
  // Matrix4::from_angle_x(Rad(1.2))
  float c = std::cos(1.2f);
  float s = std::sin(1.2f);
  math::Mat4 rot_x = math::Mat4(
      math::Vec4(1, 0, 0, 0), math::Vec4(0, c, s, 0), math::Vec4(0, -s, c, 0),
      math::Vec4(0, 0, 0, 1));

  for (int i = 0; i < 1000; i++) {
    auto e = world.spawn();
    world.insert_component(e, Transform{rot_x});
    world.insert_component(e, Position{math::Vec3(1, 0, 0)});
    world.insert_component(e, Rotation{math::Vec3(1, 0, 0)});
    world.insert_component(e, Velocity{math::Vec3(1, 0, 0)});
  }
}

static void BM_HeavyCompute(benchmark::State &state) {
  World world;
  populate_world(world);

  for (auto _ : state) {
    Query<Mut<Transform>, Mut<Position>> query(world);
    query.each([](Transform *t, Position *p) {
      for (int i = 0; i < 100; i++) {
        t->matrix = t->matrix.Inversed();
      }
      p->value = t->matrix.Multiply3x3(p->value);
    });
  }
}
BENCHMARK(BM_HeavyCompute)->MinTime(3);

} // namespace bench_heavy_compute
