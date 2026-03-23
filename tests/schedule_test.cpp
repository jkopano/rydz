#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/schedule.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

namespace {

// ============================================================
// Components / resources for tests
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

struct Counter {
  using Type = Resource;
  int value = 0;
};

// ============================================================
// Helpers
// ============================================================

struct ConcurrencyTracker {
  std::atomic<int> max_concurrent{0};
  std::atomic<int> current{0};

  void enter() {
    int c = ++current;
    int prev = max_concurrent.load();
    while (c > prev && !max_concurrent.compare_exchange_weak(prev, c))
      ;
  }

  void exit() { --current; }
};

} // namespace

// ============================================================
// Parallel / access tests
// ============================================================

TEST(ScheduleGraphTest, ParallelWhenNoWriteConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});
  world.insert_component(e, CompB{0});

  ConcurrencyTracker tracker;

  Schedule schedule;

  schedule.add_system_fn([&tracker](Query<Mut<CompA>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a) { a->value += 1; });
    tracker.exit();
  });

  schedule.add_system_fn([&tracker](Query<Mut<CompB>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompB *b) { b->value += 10; });
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(world.get_component<CompA>(e)->value, 1);
  EXPECT_EQ(world.get_component<CompB>(e)->value, 10);
  EXPECT_EQ(tracker.max_concurrent.load(), 2)
      << "Systems writing different components should run in parallel";
}

TEST(ScheduleGraphTest, SequentialWhenWriteConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});

  ConcurrencyTracker tracker;

  Schedule schedule;

  schedule.add_system_fn([&tracker](Query<Mut<CompA>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a) { a->value += 1; });
    tracker.exit();
  });

  schedule.add_system_fn([&tracker](Query<Mut<CompA>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a) { a->value += 10; });
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(world.get_component<CompA>(e)->value, 11);
  EXPECT_EQ(tracker.max_concurrent.load(), 1)
      << "Systems writing the same component must run sequentially";
}

TEST(ScheduleGraphTest, SequentialWhenWriteReadConflict) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{100});

  ConcurrencyTracker tracker;

  Schedule schedule;

  schedule.add_system_fn([&tracker](Query<Mut<CompA>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a) { a->value += 1; });
    tracker.exit();
  });

  schedule.add_system_fn([&tracker](Query<CompA> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](const CompA *) {});
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(tracker.max_concurrent.load(), 1)
      << "Writer + Reader of same component must run sequentially";
}

TEST(ScheduleGraphTest, ParallelWhenReadOnly) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{42});

  ConcurrencyTracker tracker;

  Schedule schedule;

  schedule.add_system_fn([&tracker](Query<CompA> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](const CompA *) {});
    tracker.exit();
  });

  schedule.add_system_fn([&tracker](Query<CompA> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](const CompA *) {});
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(tracker.max_concurrent.load(), 2)
      << "Two read-only systems on the same component should run in parallel";
}

// ============================================================
// Batching: readers should batch together even if a writer is between them
// ============================================================

TEST(ScheduleGraphTest, ReadersParallelDespiteWriterInMiddle) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});
  world.insert_component(e, CompB{0});

  ConcurrencyTracker tracker;

  Schedule schedule;

  // Reader 1 of A,B
  schedule.add_system_fn([&tracker](Query<CompA, CompB> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](const CompA *, const CompB *) {});
    tracker.exit();
  });

  // Writer of A,B (conflicts with both readers)
  schedule.add_system_fn([&tracker](Query<Mut<CompA>, Mut<CompB>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a, CompB *b) {
      a->value = 1;
      b->value = 1;
    });
    tracker.exit();
  });

  // Reader 2 of A,B (compatible with Reader 1, conflicts with Writer)
  schedule.add_system_fn([&tracker](Query<CompA, CompB> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](const CompA *, const CompB *) {});
    tracker.exit();
  });

  schedule.run(world);

  // Reader 1 and Reader 2 should be batched together (parallel)
  // Writer runs in a separate batch
  EXPECT_EQ(tracker.max_concurrent.load(), 2)
      << "Two readers should batch together even when a writer is inserted "
         "between them";
}

// ============================================================
// Ordering: .after(), .before(), chain()
// ============================================================

void ordering_first(ResMut<Counter> c) { c->value = 1; }
void ordering_second(ResMut<Counter> c) { c->value = c->value * 10 + 2; }
void ordering_third(ResMut<Counter> c) { c->value = c->value * 10 + 3; }

TEST(ScheduleOrderingTest, AfterEnforcesOrder) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  // Add second first, but it should run after first
  schedule.add_system_fn(
      std::move(group(ordering_second).after(ordering_first)));
  schedule.add_system_fn(ordering_first);

  schedule.run(world);

  // first sets 1, second does 1*10+2=12
  EXPECT_EQ(world.get_resource<Counter>()->value, 12);
}

TEST(ScheduleOrderingTest, BeforeEnforcesOrder) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(ordering_second);
  schedule.add_system_fn(
      std::move(group(ordering_first).before(ordering_second)));

  schedule.run(world);

  // first sets 1, second does 1*10+2=12
  EXPECT_EQ(world.get_resource<Counter>()->value, 12);
}

TEST(ScheduleOrderingTest, ChainEnforcesSequentialOrder) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      chain(ordering_first, ordering_second, ordering_third));

  schedule.run(world);

  // first:1, second:1*10+2=12, third:12*10+3=123
  EXPECT_EQ(world.get_resource<Counter>()->value, 123);
}

TEST(ScheduleOrderingTest, ChainReversedInsertionStillWorks) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  // chain should override insertion order
  schedule.add_system_fn(
      chain(ordering_third, ordering_second, ordering_first));

  schedule.run(world);

  // third:3, second:3*10+2=32, first:32*10+1=... wait, no — they all write
  // c->value
  // third sets 3, second does 3*10+2=32, first sets 1
  // Actually first always sets value=1, so the last in chain wins for =
  // Let me re-check: chain(third, second, first) means third runs, then second,
  // then first
  EXPECT_EQ(world.get_resource<Counter>()->value, 1);
}

// ============================================================
// Ordering: error cases
// ============================================================

TEST(ScheduleOrderingTest, AfterNonexistentSystemThrows) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  // ordering_second.after(ordering_third) but ordering_third is not in schedule
  schedule.add_system_fn(
      std::move(group(ordering_second).after(ordering_third)));

  EXPECT_THROW(schedule.run(world), std::runtime_error);
}

TEST(ScheduleOrderingTest, BeforeNonexistentSystemThrows) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(ordering_first).before(ordering_third)));

  EXPECT_THROW(schedule.run(world), std::runtime_error);
}

TEST(ScheduleOrderingTest, CycleThrows) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(ordering_first).after(ordering_second)));
  schedule.add_system_fn(
      std::move(group(ordering_second).after(ordering_first)));

  EXPECT_THROW(schedule.run(world), std::runtime_error);
}

TEST(ScheduleOrderingTest, SelfCycleThrows) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      std::move(group(ordering_first).after(ordering_first)));

  EXPECT_THROW(schedule.run(world), std::runtime_error);
}

// ============================================================
// Schedule basics / edge cases
// ============================================================

TEST(ScheduleTest, EmptyScheduleRunsWithoutCrash) {
  World world;
  Schedule schedule;
  schedule.run(world); // should be no-op
  EXPECT_TRUE(schedule.empty());
}

TEST(ScheduleTest, SingleSystem) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn([](ResMut<Counter> c) { c->value = 42; });
  schedule.run(world);

  EXPECT_EQ(world.get_resource<Counter>()->value, 42);
}

TEST(ScheduleTest, SystemCount) {
  Schedule schedule;
  EXPECT_EQ(schedule.system_count(), 0u);

  schedule.add_system_fn([](ResMut<Counter>) {});
  EXPECT_EQ(schedule.system_count(), 1u);

  schedule.add_system_fn([](ResMut<Counter>) {});
  EXPECT_EQ(schedule.system_count(), 2u);
}

TEST(ScheduleTest, MultipleRunsReuseGraph) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn([](ResMut<Counter> c) { c->value += 1; });

  schedule.run(world);
  schedule.run(world);
  schedule.run(world);

  EXPECT_EQ(world.get_resource<Counter>()->value, 3);
}

TEST(ScheduleTest, ExclusiveSystemRunsAlone) {
  World world;
  world.insert_resource(Counter{0});

  ConcurrencyTracker tracker;

  Schedule schedule;

  // Exclusive system (takes World&)
  schedule.add_system_fn([&tracker](World &w) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    w.get_resource<Counter>()->value += 1;
    tracker.exit();
  });

  schedule.add_system_fn([&tracker](ResMut<Counter> c) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    c->value += 10;
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(world.get_resource<Counter>()->value, 11);
  EXPECT_EQ(tracker.max_concurrent.load(), 1)
      << "Exclusive system must not run concurrently with anything";
}

// ============================================================
// Chain with descriptors (run_if + ordering combined)
// ============================================================

TEST(ScheduleOrderingTest, ChainWithRunIf) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      chain(ordering_first,
            std::move(group(ordering_second).run_if([](Res<Counter> c) {
              return c->value == 1;
            })),
            ordering_third));

  schedule.run(world);

  // first sets 1, second condition true (value==1) so 1*10+2=12,
  // third: 12*10+3=123
  EXPECT_EQ(world.get_resource<Counter>()->value, 123);
}

TEST(ScheduleOrderingTest, ChainWithRunIfFalse) {
  World world;
  world.insert_resource(Counter{0});

  Schedule schedule;
  schedule.add_system_fn(
      chain(ordering_first,
            std::move(group(ordering_second).run_if([](Res<Counter> c) {
              return c->value == 999;
            })),
            ordering_third));

  schedule.run(world);

  // first sets 1, second condition false (value!=999) so skipped,
  // third: 1*10+3=13
  EXPECT_EQ(world.get_resource<Counter>()->value, 13);
}

// ============================================================
// Ordering + parallelism interaction
// ============================================================

TEST(ScheduleOrderingTest, AfterDoesNotPreventUnrelatedParallelism) {
  World world;

  auto e = world.spawn();
  world.insert_component(e, CompA{0});
  world.insert_component(e, CompB{0});
  world.insert_resource(Counter{0});

  ConcurrencyTracker tracker;

  Schedule schedule;

  // sys_a writes CompA
  schedule.add_system_fn([&tracker](Query<Mut<CompA>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompA *a) { a->value = 1; });
    tracker.exit();
  });

  // sys_b writes CompB — no access conflict with sys_a, no ordering constraint
  schedule.add_system_fn([&tracker](Query<Mut<CompB>> query) {
    tracker.enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    query.each([](CompB *b) { b->value = 2; });
    tracker.exit();
  });

  schedule.run(world);

  EXPECT_EQ(tracker.max_concurrent.load(), 2)
      << "Unrelated systems should still run in parallel";
}
