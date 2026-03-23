// 02 - ECS Basics - Może być przydatne ale niekoniecznie
// Pokazuje: komponenty, spawn, Query, Cmd, Res<Time>, run_once, bundles,
//           chain, after, before

#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_ecs/storage.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <print>

using namespace ecs;

// structy które nie mają jakiś using Type, są automatycznie komponentami

struct Health {
  i32 value = 100;
};

struct Speed {
  // można zmienić storage który używają komponenty danego typu
  // dla komponentów które są często - default lepszy
  // dla komponentów które są często - HashMapStorage lepszy
  using Storage = HashMapStorage<Speed>;
  f32 value = 5.0f;
};

struct Position {
  f32 x = 0, y = 0;
};

struct EnemyTag {};
struct NotEnemyTag {};

// Resource - czyli basically singleton
struct EnemyCount {
  using Type = Resource;
  u32 count;
};

// Bundle - działa tak, że gdy spawnujesz Bundle, to on sam nie jest komponentem
// za to, gdy go spawnujesz to wszystkie jego pola (które powinny być
// komponentami) zostają spawnione
struct EnemyBundle {
  using Type = Bundle;
  EnemyTag tag;
  Health health;
  Speed speed;
  Position position;
};

// Startup - spawnujemy 5 enemy
// Cmd to jeden z rodzaji interakcji z światem
// Cmd powoduje że takie funkcje jak spawn, despawn etc są defferowane
// aż do momentu zakończenia działania owego Schedulera
//
// Istnieje Res<T> oraz ResMut<T> jezeli chce dostać jakis T resource
// z Mut to tak jak w komponentach, możesz go także zmieniać nie jest const
void spawn_enemies(Cmd cmd, ResMut<EnemyCount> count) {
  for (i32 i = 0; i < 5; ++i) {
    // spawn tworzy entity z konkretnymi komponentami
    cmd.spawn(EnemyBundle{
        .tag = {},
        .health = {100 + i * 10},
        .speed = {2.0f + i * 0.5f},
        .position = {i * 10.0f, 0.0f},
    });
    count->count++;
  }
  std::println("spawned {} enemies", count->count);
}

// Query to zapytanie do świata, daj mi wszystkie entity które posiadają
// Position i Speed, brak Mut powoduje że taki komponent jest const, z Mut
// możemy komponent nie tylko odczytywać ale też nadpisywać
void move_enemies(Query<Mut<Position>, Speed> query, Res<Time> time) {
  for (auto [pos, spd] : query.iter()) {
    pos->x += spd->value * time->delta_seconds;
  }
}

void apply_damage(Query<Mut<Health>> query) {
  for (auto [hp] : query.iter()) {
    if (hp->value > 0)
      hp->value -= 1;
  }
}

// Query po za tym że może prosić o konkretne komponenty, ma też coś takiego jak
// filter np: With<T>, Without<T>, Added<T>, Changed<T>, używaj tego jak chcesz
// dostać bardziej konkretne entitties, ale bez dostania komponentu T
void print_enemies(
    Query<Health, Position, Speed, With<EnemyTag>, Without<NotEnemyTag>> query,
    Res<Time> time) {
  if (static_cast<i32>(time->elapsed_seconds) % 2 != 0)
    return;

  std::println("--- enemies ---");
  for (auto [hp, pos, spd] : query.iter()) {
    std::println("  hp={} pos=({:.1f}, {:.1f}) speed={:.1f}", hp->value, pos->x,
                 pos->y, spd->value);
  }
}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "02 - ECS BAZA", 60}))
      // możemy to zrobić tutaj albo w systemach
      .insert_resource(EnemyCount{0})
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_systems(ScheduleLabel::Startup, spawn_enemies)
      // Update - wypisujemy stan co sekundę (patrz % 2 != 0)
      .add_systems(ScheduleLabel::Update, group(print_enemies))
      // to samo co wyzej ale run_if(run_once()) powoduje że ten system
      // odpali się tylko raz w Update
      .add_systems(ScheduleLabel::Update,
                   group(print_enemies).run_if(run_once()))

      // chain/after/before
      // chain() wymusza kolejność w grupie
      // bez chain() systemy w grupie mogą się odpalić w dowolnej kolejności
      .add_systems(ScheduleLabel::Update,
                   group(move_enemies, apply_damage).chain())

      // after/before
      // print_enemies odpala się po move_enemies
      // można było też napisać ...apply_damage).chain().before(print_enemies)
      .add_systems(ScheduleLabel::Update,
                   group(print_enemies).after(move_enemies))
      .run();
}
