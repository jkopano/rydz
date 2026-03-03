#include <gtest/gtest.h>

#include "rydz_ecs/event.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// Event tests (mirrors event_test.rs)
// ============================================================

struct MyEvent {
  int value;
  bool operator==(const MyEvent &o) const { return value == o.value; }
};

TEST(EventTest, DoubleBufferedEvents) {
  Events<MyEvent> events;
  auto reader_a = events.register_reader();
  auto reader_b = events.register_reader();

  // --- Frame 1: send events ---
  events.send(MyEvent{1});
  events.send(MyEvent{2});

  std::vector<MyEvent> received_a, received_b;
  events.read(reader_a, [&](const MyEvent &e) { received_a.push_back(e); });
  events.read(reader_b, [&](const MyEvent &e) { received_b.push_back(e); });

  ASSERT_EQ(received_a.size(), 2u);
  EXPECT_EQ(received_a[0], (MyEvent{1}));
  EXPECT_EQ(received_a[1], (MyEvent{2}));

  ASSERT_EQ(received_b.size(), 2u);
  EXPECT_EQ(received_b[0], (MyEvent{1}));
  EXPECT_EQ(received_b[1], (MyEvent{2}));

  events.update(); // rotate

  // --- Frame 2: read again (should get nothing new) ---
  size_t prev_a = received_a.size();
  events.read(reader_a, [&](const MyEvent &e) { received_a.push_back(e); });
  EXPECT_EQ(received_a.size(), prev_a); // no new events

  events.update(); // rotate again

  // --- Frame 3: events from frame 1 should now be gone ---
  size_t prev_b = received_b.size();
  events.read(reader_b, [&](const MyEvent &e) { received_b.push_back(e); });
  EXPECT_EQ(received_b.size(), prev_b); // still nothing new
}

TEST(EventTest, EventWriterReader) {
  World world;
  world.insert_resource(Events<MyEvent>{});

  // Create reader BEFORE writing — reader tracks from registration point
  auto *events = world.get_resource<Events<MyEvent>>();
  EventReader<MyEvent> reader(events);

  // Writer
  {
    EventWriter<MyEvent> writer(events);
    writer.send(MyEvent{10});
    writer.send(MyEvent{20});
  }

  // Reader should see events sent after registration
  std::vector<MyEvent> received;
  reader.for_each([&](const MyEvent &e) { received.push_back(e); });
  ASSERT_EQ(received.size(), 2u);
  EXPECT_EQ(received[0], (MyEvent{10}));
  EXPECT_EQ(received[1], (MyEvent{20}));
}
