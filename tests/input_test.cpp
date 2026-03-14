#include <gtest/gtest.h>

#include "rydz_ecs/input.hpp"

using namespace ecs;

TEST(InputTest, KeyPressedOnFirstFrame) {
  Input input;
  input.set_key_down(KEY_W);

  EXPECT_TRUE(input.key_down(KEY_W));
  EXPECT_TRUE(input.key_pressed(KEY_W));
  EXPECT_FALSE(input.key_released(KEY_W));
}

TEST(InputTest, KeyDownButNotPressedOnSubsequentFrame) {
  Input input;
  // Frame 1: key goes down
  input.set_key_down(KEY_W);
  EXPECT_TRUE(input.key_pressed(KEY_W));

  // Frame 2: still held, clear_frame resets pressed
  input.clear_frame();
  input.set_key_down(KEY_W);

  EXPECT_TRUE(input.key_down(KEY_W));
  EXPECT_FALSE(input.key_pressed(KEY_W)); // not freshly pressed
  EXPECT_FALSE(input.key_released(KEY_W));
}

TEST(InputTest, KeyReleased) {
  Input input;
  // Frame 1: press
  input.set_key_down(KEY_A);

  // Frame 2: release
  input.clear_frame();
  input.set_key_up(KEY_A);

  EXPECT_FALSE(input.key_down(KEY_A));
  EXPECT_FALSE(input.key_pressed(KEY_A));
  EXPECT_TRUE(input.key_released(KEY_A));
}

TEST(InputTest, ReleasedClearsNextFrame) {
  Input input;
  input.set_key_down(KEY_A);
  input.clear_frame();
  input.set_key_up(KEY_A);
  EXPECT_TRUE(input.key_released(KEY_A));

  // Next frame: released flag should be gone
  input.clear_frame();
  EXPECT_FALSE(input.key_released(KEY_A));
}

TEST(InputTest, MouseDelta) {
  Input input;
  input.set_mouse_delta(5.0f, -3.0f);

  EXPECT_FLOAT_EQ(input.mouse_delta_x(), 5.0f);
  EXPECT_FLOAT_EQ(input.mouse_delta_y(), -3.0f);

  input.clear_frame();
  EXPECT_FLOAT_EQ(input.mouse_delta_x(), 0.0f);
  EXPECT_FLOAT_EQ(input.mouse_delta_y(), 0.0f);
}

TEST(InputTest, MultipleKeys) {
  Input input;
  input.set_key_down(KEY_W);
  input.set_key_down(KEY_LEFT_SHIFT);

  EXPECT_TRUE(input.key_down(KEY_W));
  EXPECT_TRUE(input.key_down(KEY_LEFT_SHIFT));
  EXPECT_TRUE(input.key_pressed(KEY_W));
  EXPECT_TRUE(input.key_pressed(KEY_LEFT_SHIFT));

  // Frame 2: release shift, keep W
  input.clear_frame();
  input.set_key_down(KEY_W);
  input.set_key_up(KEY_LEFT_SHIFT);

  EXPECT_TRUE(input.key_down(KEY_W));
  EXPECT_FALSE(input.key_pressed(KEY_W));
  EXPECT_FALSE(input.key_down(KEY_LEFT_SHIFT));
  EXPECT_TRUE(input.key_released(KEY_LEFT_SHIFT));
}

TEST(InputTest, KeyNotPressedByDefault) {
  Input input;
  EXPECT_FALSE(input.key_down(KEY_W));
  EXPECT_FALSE(input.key_pressed(KEY_W));
  EXPECT_FALSE(input.key_released(KEY_W));
}

TEST(InputTest, InputAsResource) {
  World world;
  world.insert_resource(Input{});

  auto *input = world.get_resource<Input>();
  ASSERT_NE(input, nullptr);

  input->set_key_down(KEY_SPACE);
  EXPECT_TRUE(input->key_pressed(KEY_SPACE));

  // Simulate reading from Res<Input>
  Res<Input> res{world.get_resource<Input>()};
  EXPECT_TRUE(res->key_pressed(KEY_SPACE));
  EXPECT_TRUE(res->key_down(KEY_SPACE));
}

TEST(InputTest, InputInSystem) {
  World world;
  world.insert_resource(Input{});

  // Simulate the polling system setting keys
  auto *input = world.get_resource<Input>();
  input->set_key_down(KEY_W);
  input->set_key_down(KEY_D);
  input->set_mouse_delta(10.0f, -5.0f);

  // A consumer system reads Res<Input>
  bool w_pressed = false;
  bool d_down = false;
  float mx = 0, my = 0;

  auto consumer = make_system([&](Res<Input> inp) {
    w_pressed = inp->key_pressed(KEY_W);
    d_down = inp->key_down(KEY_D);
    mx = inp->mouse_delta_x();
    my = inp->mouse_delta_y();
  });

  consumer->run(world);

  EXPECT_TRUE(w_pressed);
  EXPECT_TRUE(d_down);
  EXPECT_FLOAT_EQ(mx, 10.0f);
  EXPECT_FLOAT_EQ(my, -5.0f);
}

TEST(InputTest, FullFrameSimulation) {
  World world;
  world.insert_resource(Input{});

  auto *input = world.get_resource<Input>();

  // === Frame 1: W pressed ===
  input->clear_frame();
  input->set_key_down(KEY_W);

  int press_count = 0;
  int down_count = 0;

  auto sys = make_system([&](Res<Input> inp) {
    if (inp->key_pressed(KEY_W))
      press_count++;
    if (inp->key_down(KEY_W))
      down_count++;
  });

  sys->run(world);
  EXPECT_EQ(press_count, 1);
  EXPECT_EQ(down_count, 1);

  // === Frame 2: W still held ===
  input->clear_frame();
  input->set_key_down(KEY_W);

  sys->run(world);
  EXPECT_EQ(press_count, 1); // not pressed again
  EXPECT_EQ(down_count, 2);  // still down

  // === Frame 3: W released ===
  input->clear_frame();
  input->set_key_up(KEY_W);

  bool released = false;
  auto release_sys = make_system([&](Res<Input> inp) {
    released = inp->key_released(KEY_W);
  });

  release_sys->run(world);
  EXPECT_TRUE(released);
}
