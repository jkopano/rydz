// 04 - Events
// Pokazuje: add_event, EventWriter, EventReader, wysyłanie i odbieranie

#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <print>

using namespace ecs;

// NA PRZYSZŁOŚĆ DODAĆ using T = Event;

struct DamageEvent {
  using T = Event;
  i32 amount;
  const char *source;
};

struct ScoreEvent {
  using T = Event;
  i32 points;
};

// System wysyłający eventy na naciśnięcie klawisza
void send_events(EventWriter<DamageEvent> dmg_writer,
                 EventWriter<ScoreEvent> score_writer, Res<Input> input) {

  if (input->key_pressed(KEY_A)) {
    dmg_writer.send(DamageEvent{10, "miecz"});
    dmg_writer.send(DamageEvent{10, "miecz"});
    dmg_writer.send(DamageEvent{10, "miecz"});
  }

  if (input->key_pressed(KEY_S))
    dmg_writer.send(DamageEvent{25, "łuk"});

  if (input->key_pressed(KEY_D))
    score_writer.send(ScoreEvent{100});
}

// Systemy czytające eventy
void read_damage(EventReader<DamageEvent> reader) {
  reader.for_each([](const DamageEvent &e) {
    std::println("damage: {} from {}", e.amount, e.source);
  });
}

void read_score(EventReader<ScoreEvent> reader) {
  reader.for_each(
      [](const ScoreEvent &e) { std::println("score: +{}", e.points); });
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "04 - Events", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(input_plugin)
      .add_event<DamageEvent>()
      .add_event<ScoreEvent>()
      .add_systems(ScheduleLabel::Update, send_events)
      .add_systems(ScheduleLabel::Update, group(read_damage).after(send_events))
      .add_systems(ScheduleLabel::Update, group(read_score).after(send_events))
      .run();
}
