#include <gtest/gtest.h>

#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/state.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// State enum
// ============================================================

enum class GameState { Menu, Playing };

// ============================================================
// State tests (mirrors state_test.rs)
// ============================================================

TEST(StateTest, InStateCondition) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  // in_state should return true for current state
  auto cond_menu = make_condition(in_state(GameState::Menu));
  EXPECT_TRUE(cond_menu->is_true(world));

  auto cond_playing = make_condition(in_state(GameState::Playing));
  EXPECT_FALSE(cond_playing->is_true(world));
}

TEST(StateTest, StateTransition) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  // Request transition
  auto *next = world.get_resource<NextState<GameState>>();
  ASSERT_NE(next, nullptr);
  next->set(GameState::Playing);

  // Apply transition
  check_state_transitions<GameState>(world);

  // Verify state changed
  auto *state = world.get_resource<State<GameState>>();
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->get(), GameState::Playing);
}

TEST(StateTest, StateTransitionNoOp) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  // No pending transition
  check_state_transitions<GameState>(world);

  // State should remain unchanged
  auto *state = world.get_resource<State<GameState>>();
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->get(), GameState::Menu);
}

TEST(StateTest, ConditionedSystemWithState) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  int menu_runs = 0;
  int playing_runs = 0;

  auto menu_sys =
      make_system_run_if([&]() { menu_runs++; }, in_state(GameState::Menu));

  auto playing_sys = make_system_run_if([&]() { playing_runs++; },
                                        in_state(GameState::Playing));

  menu_sys->run(world);
  playing_sys->run(world);
  EXPECT_EQ(menu_runs, 1);
  EXPECT_EQ(playing_runs, 0);

  // Transition to Playing
  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  menu_sys->run(world);
  playing_sys->run(world);
  EXPECT_EQ(menu_runs, 1);    // didn't run again
  EXPECT_EQ(playing_runs, 1); // now runs
}
