// 12 - Observers
// Pokazuje: add_event, add_observer, EntityCommands::observe i trigger

#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_platform/mod.hpp"
#include <print>

using namespace ecs;

struct Hp {
  i32 value;
};

struct PlayerTag {};

struct LogEvent {
  using T = Event;
  const char *label;
};

struct DamageEvent {
  using T = EntityEvent;
  Entity target;
  i32 amount{};
};

void setup(Cmd cmd) {
  auto player = cmd.spawn(PlayerTag{}, Hp{100});
  player.observe([](On<DamageEvent> event, Query<Mut<Hp>> query) {
    if (auto hp = query.get(event->target)) {
      hp->value -= event->amount;
      std::println("player hp: {}", hp->value);
    }
  });
  cmd.spawn(PlayerTag{}, Hp{100});
}

void trigger_events(Cmd cmd, Res<Input> input,
                    Query<Entity, PlayerTag> players) {
  if (players.is_empty()) {
    return;
  }

  auto [player_ent, _] = players.iter().front();

  if (input->key_pressed(KEY_A)) {
    cmd.trigger(LogEvent{"global log event"});
  }

  if (input->key_pressed(KEY_D)) {
    cmd.trigger(DamageEvent{.target = player_ent, .amount = 10});
  }
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
                     .window = {.width = 800,
                                .height = 600,
                                .title = "12 - Observers",
                                .target_fps = 60},
                 }))
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_event<LogEvent>()
      .add_event<DamageEvent>()
      .add_observer(
          [](On<LogEvent> event) { std::println("log: {}", event->label); })
      .add_observer([](On<DamageEvent> event) {
        std::println("global observer: {} damage for entity {}", event->amount,
                     event->target.index());
      })
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Update, trigger_events)
      .run();
}
