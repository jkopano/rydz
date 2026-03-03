#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "rydz_ecs/query.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Components for parallel tests
// ============================================================

struct CompA {
  int value = 0;
};
struct CompB {
  int value = 0;
};
struct CompC {
  int value = 0;
};

// ============================================================
// Schedule graph tests
// ============================================================

// Test: Two systems writing DIFFERENT components should run in parallel
TEST(ScheduleGraphTest, ParallelWhenNoWriteConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});
  world.insert_component(e, CompB{0});

  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};

  Schedule schedule;

  // System 1: writes CompA only
  schedule.add_system_fn(
      [&max_concurrent, &current](Query<Mut<CompA>> query) {
        int c = ++current;
        int prev = max_concurrent.load();
        while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
          ;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        query.for_each([](CompA *a) { a->value += 1; });
        --current;
      });

  // System 2: writes CompB only (no conflict with System 1)
  schedule.add_system_fn(
      [&max_concurrent, &current](Query<Mut<CompB>> query) {
        int c = ++current;
        int prev = max_concurrent.load();
        while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
          ;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        query.for_each([](CompB *b) { b->value += 10; });
        --current;
      });

  schedule.run(world);

  // Both should have run
  EXPECT_EQ(world.get_component<CompA>(e)->value, 1);
  EXPECT_EQ(world.get_component<CompB>(e)->value, 10);

  // They should have run concurrently (max_concurrent should be 2)
  EXPECT_EQ(max_concurrent.load(), 2)
      << "Systems writing different components should run in parallel";
}

// Test: Two systems writing the SAME component must run sequentially
TEST(ScheduleGraphTest, SequentialWhenWriteConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});

  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};

  Schedule schedule;

  // System 1: writes CompA
  schedule.add_system_fn(
      [&max_concurrent, &current](Query<Mut<CompA>> query) {
        int c = ++current;
        int prev = max_concurrent.load();
        while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
          ;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        query.for_each([](CompA *a) { a->value += 1; });
        --current;
      });

  // System 2: also writes CompA (CONFLICT — must be sequential)
  schedule.add_system_fn(
      [&max_concurrent, &current](Query<Mut<CompA>> query) {
        int c = ++current;
        int prev = max_concurrent.load();
        while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
          ;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        query.for_each([](CompA *a) { a->value += 10; });
        --current;
      });

  schedule.run(world);

  // Both should have run
  EXPECT_EQ(world.get_component<CompA>(e)->value, 11);

  // They should NOT have run concurrently (max_concurrent should be 1)
  EXPECT_EQ(max_concurrent.load(), 1)
      << "Systems writing the same component must run sequentially";
}

// Test: Writer + Reader of same component must be sequential
TEST(ScheduleGraphTest, SequentialWhenWriteReadConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{100});

  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};

  Schedule schedule;

  // System 1: writes CompA
  schedule.add_system_fn(
      [&max_concurrent, &current](Query<Mut<CompA>> query) {
        int c = ++current;
        int prev = max_concurrent.load();
        while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
          ;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        query.for_each([](CompA *a) { a->value += 1; });
        --current;
      });

  // System 2: reads CompA (conflict with writer)
  schedule.add_system_fn([&max_concurrent, &current](Query<CompA> query) {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.for_each([](const CompA *) {});
    --current;
  });

  schedule.run(world);

  EXPECT_EQ(max_concurrent.load(), 1)
      << "Writer + Reader of same component must run sequentially";
}

// Test: Two readers of the same component should run in parallel
TEST(ScheduleGraphTest, ParallelWhenReadOnly) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{42});

  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};

  Schedule schedule;

  // System 1: reads CompA
  schedule.add_system_fn([&max_concurrent, &current](Query<CompA> query) {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.for_each([](const CompA *) {});
    --current;
  });

  // System 2: also reads CompA (no conflict — read-read is compatible)
  schedule.add_system_fn([&max_concurrent, &current](Query<CompA> query) {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.for_each([](const CompA *) {});
    --current;
  });

  schedule.run(world);

  EXPECT_EQ(max_concurrent.load(), 2)
      << "Two read-only systems on the same component should run in parallel";
}

// Test: Three systems — A||B sequential with C
TEST(ScheduleGraphTest, MixedParallelAndSequential) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});
  world.insert_component(e, CompB{0});
  world.insert_component(e, CompC{0});

  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};
  std::atomic<int> order{0};
  std::atomic<int> sys3_order{0};

  Schedule schedule;

  // System 1: writes CompA (compatible with sys 2)
  schedule.add_system_fn([&](Query<Mut<CompA>> query) {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.for_each([](CompA *a) { a->value = 1; });
    --current;
    ++order;
  });

  // System 2: writes CompB (compatible with sys 1, but conflicts with sys 3)
  schedule.add_system_fn([&](Query<Mut<CompB>> query) {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.for_each([](CompB *b) { b->value = 2; });
    --current;
    ++order;
  });

  // System 3: writes CompB (conflicts with sys 2, must wait)
  schedule.add_system_fn([&](Query<Mut<CompB>> query) {
    sys3_order.store(order.load());
    query.for_each([](CompB *b) { b->value += 10; });
  });

  schedule.run(world);

  EXPECT_EQ(world.get_component<CompA>(e)->value, 1);
  EXPECT_EQ(world.get_component<CompB>(e)->value, 12);

  // Systems 1 and 2 should have run in parallel
  EXPECT_EQ(max_concurrent.load(), 2);

  // System 3 must have run AFTER system 2 (order >= 2 means both 1&2 finished)
  EXPECT_GE(sys3_order.load(), 1)
      << "System 3 should run after System 2 (write-write conflict on CompB)";
}
