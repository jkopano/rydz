#include <gtest/gtest.h>

#include "rydz_ecs/mod.hpp"

using namespace ecs;

namespace {

struct MyMessage {
  using T = Message;
  int value;
  bool operator==(const MyMessage &o) const { return value == o.value; }
};

struct PingEvent {
  using T = Event;
  int value;
};

struct HitEvent {
  using T = EntityEvent;
  Entity target;
  int amount;
};

struct Counter {
  using T = Resource;
  int value = 0;
};

struct Health {
  int value;
};

struct Marker {};

} // namespace

TEST(EventTest, DoubleBufferedMessages) {
  Messages<MyMessage> messages;
  auto reader_a = messages.register_reader();
  auto reader_b = messages.register_reader();

  messages.send(MyMessage{1});
  messages.send(MyMessage{2});

  std::vector<MyMessage> received_a, received_b;
  for (const auto &message : messages.iter(reader_a)) {
    received_a.push_back(message);
  }
  for (const auto &message : messages.iter(reader_b)) {
    received_b.push_back(message);
  }

  ASSERT_EQ(received_a.size(), 2u);
  EXPECT_EQ(received_a[0], (MyMessage{1}));
  EXPECT_EQ(received_a[1], (MyMessage{2}));

  ASSERT_EQ(received_b.size(), 2u);
  EXPECT_EQ(received_b[0], (MyMessage{1}));
  EXPECT_EQ(received_b[1], (MyMessage{2}));

  messages.update();

  size_t prev_a = received_a.size();
  for (const auto &message : messages.iter(reader_a)) {
    received_a.push_back(message);
  }
  EXPECT_EQ(received_a.size(), prev_a);

  messages.update();

  size_t prev_b = received_b.size();
  for (const auto &message : messages.iter(reader_b)) {
    received_b.push_back(message);
  }
  EXPECT_EQ(received_b.size(), prev_b);
}

TEST(EventTest, MessageWriterReader) {
  World world;
  world.insert_resource(Messages<MyMessage>{});

  auto *messages = world.get_resource<Messages<MyMessage>>();
  MessageReader<MyMessage> reader(messages);

  {
    MessageWriter<MyMessage> writer(messages);
    writer.send(MyMessage{10});
    writer.send(MyMessage{20});
  }

  std::vector<MyMessage> received;
  for (const auto &message : reader.iter()) {
    received.push_back(message);
  }
  ASSERT_EQ(received.size(), 2u);
  EXPECT_EQ(received[0], (MyMessage{10}));
  EXPECT_EQ(received[1], (MyMessage{20}));
}

TEST(EventTest, AppAddObserverTriggersReactiveEvent) {
  App app;
  app.insert_resource(Counter{});
  app.add_event<PingEvent>().add_observer(
      [](On<PingEvent> event, ResMut<Counter> counter) {
        counter->value += event->value;
      });

  app.world().trigger(PingEvent{7});

  auto *counter = app.world().get_resource<Counter>();
  ASSERT_NE(counter, nullptr);
  EXPECT_EQ(counter->value, 7);
}

TEST(EventTest, ReactiveObserverCanUseCmd) {
  World world;
  world.insert_resource(CommandQueues{});
  world.add_event<PingEvent>();
  world.add_observer([](On<PingEvent>, Cmd cmd) { cmd.spawn(Marker{}); });

  world.trigger(PingEvent{1});

  auto *queues = world.get_resource<CommandQueues>();
  ASSERT_NE(queues, nullptr);
  queues->apply(world);

  auto *markers = world.get_storage<Marker>();
  ASSERT_NE(markers, nullptr);
  EXPECT_EQ(markers->size(), 1u);
}

TEST(EventTest, EntityEventTriggersGlobalAndEntityObservers) {
  World world;
  world.insert_resource(CommandQueues{});
  world.insert_resource(Counter{});
  world.add_event<HitEvent>();

  Entity target = world.spawn();
  world.insert_component(target, Health{100});

  world.add_observer([](On<HitEvent> event, ResMut<Counter> counter) {
    counter->value += event->amount;
  });

  {
    auto *queues = world.get_resource<CommandQueues>();
    ASSERT_NE(queues, nullptr);

    Cmd cmd(queues, &world.entities);
    cmd.entity(target).observe(
        [](On<HitEvent> event, Query<Entity, Mut<Health>> query) {
          query.each([&](Entity entity, Mut<Health> health) {
            if (entity == event->target) {
              health->value -= event->amount;
            }
          });
        });
    cmd.trigger(HitEvent{target, 5});
  }

  auto *queues = world.get_resource<CommandQueues>();
  ASSERT_NE(queues, nullptr);
  queues->apply(world);

  auto *counter = world.get_resource<Counter>();
  ASSERT_NE(counter, nullptr);
  EXPECT_EQ(counter->value, 5);

  auto *health = world.get_component<Health>(target);
  ASSERT_NE(health, nullptr);
  EXPECT_EQ(health->value, 95);

  auto *observed_by = world.get_component<ObservedBy>(target);
  ASSERT_NE(observed_by, nullptr);
  EXPECT_EQ(observed_by->observers.size(), 1u);
}

TEST(EventTest, DespawningObservedEntityCleansUpObservers) {
  World world;
  world.add_event<HitEvent>();

  Entity target = world.spawn();
  Entity observer = world.add_observer(target, [](On<HitEvent>) {});

  auto *observed_by = world.get_component<ObservedBy>(target);
  ASSERT_NE(observed_by, nullptr);
  ASSERT_EQ(observed_by->observers.size(), 1u);
  EXPECT_EQ(observed_by->observers.front(), observer);

  world.despawn(target);

  EXPECT_FALSE(world.entities.is_alive(target));
  EXPECT_FALSE(world.entities.is_alive(observer));
}
