#include <gtest/gtest.h>

#include "rydz_ecs/condition.hpp"
#include "rydz_ecs/state.hpp"
#include "rydz_ecs/system.hpp"
#include "rydz_ecs/world.hpp"

using namespace ecs;

// ============================================================
// State enums
// ============================================================

enum class GameState { Menu, Playing, Paused };

// ============================================================
// Basic state tests
// ============================================================

TEST(StateTest, InStateCondition) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  auto cond_menu = make_condition(in_state(GameState::Menu));
  EXPECT_TRUE(cond_menu->is_true(world));

  auto cond_playing = make_condition(in_state(GameState::Playing));
  EXPECT_FALSE(cond_playing->is_true(world));
}

TEST(StateTest, StateTransition) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  auto *next = world.get_resource<NextState<GameState>>();
  ASSERT_NE(next, nullptr);
  next->set(GameState::Playing);

  check_state_transitions<GameState>(world);

  auto *state = world.get_resource<State<GameState>>();
  ASSERT_NE(state, nullptr);
  EXPECT_EQ(state->get(), GameState::Playing);
}

TEST(StateTest, StateTransitionNoOp) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  check_state_transitions<GameState>(world);

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

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  menu_sys->run(world);
  playing_sys->run(world);
  EXPECT_EQ(menu_runs, 1);
  EXPECT_EQ(playing_runs, 1);
}

// ============================================================
// Edge cases
// ============================================================

TEST(StateTest, TransitionToSameStateIsNoOp) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});

  auto *next = world.get_resource<NextState<GameState>>();
  next->set(GameState::Menu); // same state

  check_state_transitions<GameState>(world);

  auto *event = world.get_resource<StateTransitionEvent<GameState>>();
  EXPECT_FALSE(event->changed)
      << "Transition to the same state should not fire changed event";
}

TEST(StateTest, MultipleTransitionsOnlyLastApplies) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  auto *next = world.get_resource<NextState<GameState>>();
  next->set(GameState::Playing);
  next->set(GameState::Paused); // overwrite

  check_state_transitions<GameState>(world);

  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Paused);
}

TEST(StateTest, TransitionClearsNextState) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});

  auto *next = world.get_resource<NextState<GameState>>();
  next->set(GameState::Playing);

  check_state_transitions<GameState>(world);

  EXPECT_FALSE(next->pending.has_value())
      << "NextState should be cleared after transition";
}

TEST(StateTest, DoubleCheckTransitionsIsIdempotent) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});

  auto *next = world.get_resource<NextState<GameState>>();
  next->set(GameState::Playing);

  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Playing);

  // Second check with no new pending — should not change anything
  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Playing);

  // changed should be reset to false on second call
  auto *event = world.get_resource<StateTransitionEvent<GameState>>();
  EXPECT_FALSE(event->changed);
}

TEST(StateTest, StateChangedCondition) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});

  auto cond = make_condition(state_changed<GameState>());

  // No transition yet
  EXPECT_FALSE(cond->is_true(world));

  // Trigger transition
  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  EXPECT_TRUE(cond->is_true(world));

  // After another check (no new transition), changed resets
  check_state_transitions<GameState>(world);
  EXPECT_FALSE(cond->is_true(world));
}

TEST(StateTest, OnEnterScheduleRuns) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int enter_playing_count = 0;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_enter[GameState::Playing].add_system_fn(
      [&enter_playing_count]() { enter_playing_count++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  EXPECT_EQ(enter_playing_count, 1);
}

TEST(StateTest, OnExitScheduleRuns) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int exit_menu_count = 0;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_exit[GameState::Menu].add_system_fn(
      [&exit_menu_count]() { exit_menu_count++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  EXPECT_EQ(exit_menu_count, 1);
}

TEST(StateTest, OnExitRunsBeforeOnEnter) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int order_counter = 0;
  int exit_order = -1;
  int enter_order = -1;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_exit[GameState::Menu].add_system_fn(
      [&]() { exit_order = order_counter++; });
  schedules->on_enter[GameState::Playing].add_system_fn(
      [&]() { enter_order = order_counter++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  EXPECT_LT(exit_order, enter_order)
      << "OnExit should run before OnEnter during transition";
}

TEST(StateTest, OnTransitionScheduleRuns) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int transition_count = 0;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_transition.add_system_fn(
      [&transition_count]() { transition_count++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(transition_count, 1);

  world.get_resource<NextState<GameState>>()->set(GameState::Paused);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(transition_count, 2);
}

TEST(StateTest, OnTransitionNotCalledForSameState) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int transition_count = 0;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_transition.add_system_fn(
      [&transition_count]() { transition_count++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Menu);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(transition_count, 0);
}

TEST(StateTest, OnTransitionOrderIsExitTransitionEnter) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int order_counter = 0;
  int exit_order = -1;
  int transition_order = -1;
  int enter_order = -1;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_exit[GameState::Menu].add_system_fn(
      [&]() { exit_order = order_counter++; });
  schedules->on_transition.add_system_fn(
      [&]() { transition_order = order_counter++; });
  schedules->on_enter[GameState::Playing].add_system_fn(
      [&]() { enter_order = order_counter++; });

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);

  EXPECT_LT(exit_order, transition_order)
      << "OnExit should run before OnTransition";
  EXPECT_LT(transition_order, enter_order)
      << "OnTransition should run before OnEnter";
}

TEST(StateTest, OnEnterNotCalledForSameState) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});
  world.insert_resource(StateSchedules<GameState>{});

  int enter_count = 0;

  auto *schedules = world.get_resource<StateSchedules<GameState>>();
  schedules->on_enter[GameState::Menu].add_system_fn(
      [&enter_count]() { enter_count++; });

  // Transition to same state
  world.get_resource<NextState<GameState>>()->set(GameState::Menu);
  check_state_transitions<GameState>(world);

  EXPECT_EQ(enter_count, 0)
      << "OnEnter should not fire when transitioning to same state";
}

TEST(StateTest, ChainedTransitions) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  world.insert_resource(NextState<GameState>{});
  world.insert_resource(StateTransitionEvent<GameState>{});

  // Menu -> Playing
  world.get_resource<NextState<GameState>>()->set(GameState::Playing);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Playing);

  // Playing -> Paused
  world.get_resource<NextState<GameState>>()->set(GameState::Paused);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Paused);

  // Paused -> Menu
  world.get_resource<NextState<GameState>>()->set(GameState::Menu);
  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Menu);
}

TEST(StateTest, MissingStateResourceHandledGracefully) {
  World world;
  // No State<GameState> inserted — only NextState
  world.insert_resource(NextState<GameState>{});

  world.get_resource<NextState<GameState>>()->set(GameState::Playing);

  // Should not crash
  check_state_transitions<GameState>(world);
}

TEST(StateTest, MissingNextStateResourceHandledGracefully) {
  World world;
  world.insert_resource(State<GameState>(GameState::Menu));
  // No NextState<GameState> inserted

  // Should not crash
  check_state_transitions<GameState>(world);
  EXPECT_EQ(world.get_resource<State<GameState>>()->get(), GameState::Menu);
}
