// 11 - System Sets
// Sety grupują systemy i dają możliwość wspólnego orderingu/warunki

#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <print>

using namespace ecs;

// Enum set — kilka setów jako enum
enum class GameSet { Input, Logic, Render };

// Struct set — jeden struct - jeden set
struct DebugSet {
  using Type = Set;
};

void input_system(Res<Input> input) {
  if (input->key_pressed(KEY_SPACE))
    std::println("[Input] SPACE pressed");
}

void movement_system(Res<Time> time) {
  // tu byłby ruch
}

void collision_system() {
  // tu byłyby kolizje
}

void debug_overlay() { std::println("[Debug] frame"); }

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "11 - System Sets", 60}))
      .add_plugin(time_plugin)
      .add_plugin(render_plugin)
      .add_plugin(input_plugin)

      // Systemy w setach enum
      .add_systems(ScheduleLabel::Update,
                   group(input_system).in_set(set(GameSet::Input)))
      .add_systems(
          ScheduleLabel::Update,
          group(movement_system, collision_system).in_set(set(GameSet::Logic)))

      // Struct set
      .add_systems(ScheduleLabel::Update,
                   group(debug_overlay).in_set(set<DebugSet>()))

      // kolejność
      // input -> logic (potem debug)
      .configure_set(ScheduleLabel::Update,
                     configure(GameSet::Input).before(set(GameSet::Logic)))
      .configure_set(ScheduleLabel::Update,
                     configure<DebugSet>().after(set(GameSet::Logic)))

      // Set z warunkiem — debug tylko gdy trzymasz D (btw as you can see, nwm
      // czy to było wczesniej w examplu, można lambdy wrzucać jako systemy)
      .configure_set(ScheduleLabel::Update,
                     configure<DebugSet>().run_if([&](Res<Input> input) {
                       return input->key_down(KEY_D);
                     }))

      .run();
}
