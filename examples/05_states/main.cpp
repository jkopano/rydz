// 05 - States - Dobre gówno polecam
// Pokazuje: init_state, in_state, NextState, OnEnter, OnExit, OnTransition

#include "math.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_graphics/rydz_graphics.hpp"
#include <print>

using namespace ecs;
using namespace math;

enum struct GameState { Menu, Playing, Paused };

// struct GameState {
//   struct Menu;
//   struct Playing;
//   struct Paused;
// };

void on_enter_menu(Cmd cmd) { std::println("[STATE] entered: Menu"); }
void on_enter_playing(Cmd cmd) { std::println("[STATE] entered: Playing"); }
void on_enter_paused(Cmd cmd) { std::println("[STATE] entered: Paused"); }
void on_exit_menu() { std::println("[STATE] exited: Menu"); }
void on_any_transition() { std::println("[STATE] transition"); }

void state_input(Res<State<GameState>> state, ResMut<NextState<GameState>> next,
                 Res<Input> input) {
  auto current = state->get();

  if (current == GameState::Menu && input->key_pressed(KEY_ENTER)) {
    next->set(GameState::Playing);
    std::println("Menu -> Playing");
  }

  if (current == GameState::Playing && input->key_pressed(KEY_P)) {
    next->set(GameState::Paused);
    std::println("Playing -> Paused");
  }

  if (current == GameState::Paused && input->key_pressed(KEY_P)) {
    next->set(GameState::Playing);
    std::println("Paused -> Playing");
  }

  if (current != GameState::Menu && input->key_pressed(KEY_M)) {
    next->set(GameState::Menu);
    std::println("-> Menu");
  }
}

// tu byłby gameplay
void game_logic(Res<Time> time) {}

// tu byłoby menu
void menu_logic() {}

int main() {
  App app;
  app.add_plugin(window_plugin({800, 600, "05 - States (Enter/P/M)", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(input_plugin)
      .init_state(GameState::Menu)
      // OnEnter/OnExit - uruchamiane przy odpowiednio Wejściu w stan i Wyjściu
      // z stanu
      .add_systems(OnEnter(GameState::Menu), on_enter_menu)
      .add_systems(OnEnter(GameState::Playing), on_enter_playing)
      .add_systems(OnEnter(GameState::Paused), on_enter_paused)
      .add_systems(OnExit(GameState::Menu), on_exit_menu)
      // OnTransition - uruchamiane przy każdej zmianie stanu
      .add_systems(OnTransition<GameState>{}, on_any_transition)
      // Systemy z warunkiem in_state
      .add_systems(ScheduleLabel::Update, state_input)
      .add_systems(ScheduleLabel::Update,
                   group(game_logic).run_if(in_state(GameState::Playing)))
      .add_systems(ScheduleLabel::Update,
                   group(menu_logic).run_if(in_state(GameState::Menu)))
      .run();
}
