// 04 - Messages
// Pokazuje: add_message, MessageWriter, MessageReader, wysyłanie i odbieranie

#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <print>

using namespace ecs;

struct DamageMessage {
  using T = Message;
  i32 amount;
  const char *source;
};

struct ScoreMessage {
  using T = Message;
  i32 points;
};

// System wysyłający message na naciśnięcie klawisza
void send_messages(MessageWriter<DamageMessage> dmg_writer,
                   MessageWriter<ScoreMessage> score_writer, Res<Input> input) {

  if (input->key_pressed(KEY_A)) {
    dmg_writer.send(DamageMessage{10, "miecz"});
    dmg_writer.send(DamageMessage{10, "miecz"});
    dmg_writer.send(DamageMessage{10, "miecz"});
  }

  if (input->key_pressed(KEY_S))
    dmg_writer.send(DamageMessage{25, "łuk"});

  if (input->key_pressed(KEY_D))
    score_writer.send(ScoreMessage{100});
}

// Systemy czytające message
void read_damage(MessageReader<DamageMessage> reader) {
  reader.for_each([](const DamageMessage &e) {
    std::println("damage: {} from {}", e.amount, e.source);
  });
}

void read_score(MessageReader<ScoreMessage> reader) {
  reader.for_each([](auto &e) { std::println("score: +{}", e.points); });
}

// Local - Parametr systemy działający w podobie do statycznych obiektów
// tyle że nie.
//
// statyczna zmienna byłaby dzielona dla różnych instancji systemu w świecie
// Local jest osobny dla każdej instacji systemu w świecie
void score_acc(MessageReader<ScoreMessage> reader, Local<i32> acc) {
  reader.for_each([&](auto &e) {
    *acc += e.points;
    std::println("score: +{}", *acc);
  });
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "04 - Messages", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_message<DamageMessage>()
      .add_message<ScoreMessage>()
      .add_systems(ScheduleLabel::Update, send_messages)
      .add_systems(ScheduleLabel::Update,
                   group(read_damage).after(send_messages))
      .add_systems(ScheduleLabel::Update,
                   group(score_acc, read_score).after(send_messages))
      .run();
}
