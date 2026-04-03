// 11 - System Sets
// Sety grupują systemy i dają możliwość wspólnego orderingu/warunki

#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include <print>

using namespace ecs;

// Enum set — kilka setów jako enum
namespace GameSet {
struct Input : Set {};
struct Logic : Set {};
struct Render : Set {};
}; // namespace GameSet

// Struct set — jeden struct - jeden set
struct DebugSet : Set {};

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
      .add_systems(GameSet::Input, group(input_system))
      .add_systems(GameSet::Logic,
                   group(movement_system, collision_system))

      // Struct set
      .add_systems(DebugSet{}, group(debug_overlay))

      // kolejność
      // input -> logic (potem debug)
      .configure_set(ScheduleLabel::Update,
                     configure(GameSet::Input, GameSet::Logic).chain())
      .configure_set(ScheduleLabel::Update,
                     configure<DebugSet>().after(set<GameSet::Logic>()))

      // Set z warunkiem — debug tylko gdy trzymasz D (btw as you can see, nwm
      // czy to było wczesniej w examplu, można lambdy wrzucać jako systemy)
      .configure_set(ScheduleLabel::Update,
                     configure<DebugSet>().run_if([](Res<Input> input) {
                       return input->key_down(KEY_D);
                     }))

      .run();
}
