// 11 - System Sets
// Sety grupują systemy i dają możliwość wspólnego orderingu/warunki

#include "rl.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_platform/mod.hpp"
#include <print>

using namespace ecs;

// Enum set — kilka setów jako enum
namespace GameSet {
struct Input : Set {};
struct Logic : Set {};
struct Render : Set {};
}; // namespace GameSet

enum PlayingSet {
  dupa,
};

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
  app.add_plugin(rydz_platform::RayPlugin::install({
                     .window = {800, 600, "11 - System Sets", 60},
                 }))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)

      // Systemy w setach enum
      // .add_systems(GameSet::Input{}, group(input_system))
      .add_systems(GameSet::Logic{}, group(movement_system, collision_system))

      // Struct set
      .add_systems(DebugSet{}, group(debug_overlay))
      .add_systems(PlayingSet::dupa, input_system)

      // kolejność
      // input -> logic (potem debug)
      .configure_set(ScheduleLabel::Update,
                     configure(GameSet::Input{}, GameSet::Logic{}).chain())
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
