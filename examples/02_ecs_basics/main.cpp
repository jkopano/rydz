// 02 - ECS Basics - Może być przydatne ale niekoniecznie
// Pokazuje: komponenty, spawn, Query, Cmd, Res<Time>, run_once, bundles,
//           chain, after, before

#include "rl.hpp"
#include "rydz_console/command_api.hpp"
#include "rydz_console/console.hpp"
#include "rydz_console/scripting.hpp"
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
  // dla komponentów które są rzadko - HashMapStorage lepszy
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
  using T = Resource;
  u32 count;
};

// Bundle - działa tak, że gdy spawnujesz Bundle, to on sam nie jest komponentem
// za to, gdy go spawnujesz to wszystkie jego pola (które powinny być
// komponentami) zostają spawnione
struct EnemyBundle {
  using T = Bundle;
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
void spawn_enemies(Cmd cmd, ResMut<EnemyCount> count, i32 amount) {
  for (i32 i = 0; i < amount; ++i) {
    cmd.spawn(EnemyBundle{
        .tag = {},
        .health = {100 + i * 10},
        .speed = {2.0f + i * 0.5f},
        .position = {i * 10.0f, 0.0f},
    });
    count->count++;
  }
  std::println("spawned {} enemies. Total: {}", amount, count->count);
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

// =========================================================================
// 1. CZYSTA FUNKCJA DOCELOWA
// Nie ma pojęcia o istnieniu Lua ani konsoli. Operuje tylko na silniku.
// =========================================================================
void kill_all_enemies_logic(Query<Mut<Health>, With<EnemyTag>> query) {
  for (auto [hp] : query.iter()) {
    hp->value = 0;
  }
}

// =========================================================================
// 2. OPAKOWANIE (SYSTEM BINDUJĄCY)
// Zespaja czystą funkcję z wirtualną maszyną Lua.
// =========================================================================
void bind_lua_commands(World &world) {

  // Rejestracja komendy bez parametrów
  engine::ConsoleAPI::bind_system(world, "kill_all", kill_all_enemies_logic);

  // Rejestracja komendy z parametrem int
  engine::BindCommand<int>::to(world, "spawn", [](int amount) {
    return [amount](Cmd cmd, ResMut<EnemyCount> count) {
      spawn_enemies(std::move(cmd), count, amount);
    };
  });
}

int main() {
  App app;
  app.add_plugin(Window::install({800, 600, "02 - ECS BAZA", 60}))
      // możemy to zrobić tutaj albo w systemach
      .insert_resource(EnemyCount{0})
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(engine::scripting_plugin)
      .add_plugin(engine::console_plugin)
      .add_systems(ScheduleLabel::Startup, bind_lua_commands)
      // Update - wypisujemy stan co sekundę (patrz % 2 != 0)
      .add_systems(ScheduleLabel::Update, group(print_enemies))
      // to samo co wyzej ale run_if(run_once()) powoduje że ten system
      // odpali się tylko raz w Update
      .add_systems(ScheduleLabel::Update,
                   group(print_enemies).run_if(run_once()))

      // chain/after/before
      // chain() wymusza kolejność w grupie
      // bez chain() systemy w grupie mogą się odpalić w dowolnej kolejności
      .add_systems(ScheduleLabel::Update, group(move_enemies))

      // after/before
      // print_enemies odpala się po move_enemies
      // można było też napisać ...apply_damage).chain().before(print_enemies)
      .add_systems(ScheduleLabel::Update,
                   group(print_enemies).after(move_enemies))
      .run();
}
