#pragma once
#include "app.hpp"
#include <unordered_set>
#include <vector>

namespace ecs {

using KeyCode = i32;

struct MouseState {
  f32 delta_x = 0.0f;
  f32 delta_y = 0.0f;
};

struct Input {
  using Type = Resource;
  bool key_down(KeyCode key) const { return keys_down_.contains(key); }
  bool key_pressed(KeyCode key) const { return keys_pressed_.contains(key); }
  bool key_released(KeyCode key) const { return keys_released_.contains(key); }

  Vector2 mouse_delta() const {
    return Vector2{mouse_.delta_x, mouse_.delta_y};
  }
  f32 mouse_delta_x() const { return mouse_.delta_x; }
  f32 mouse_delta_y() const { return mouse_.delta_y; }

  void clear_frame() {
    keys_pressed_.clear();
    keys_released_.clear();
    mouse_.delta_x = 0.0f;
    mouse_.delta_y = 0.0f;
  }

  void set_key_down(KeyCode key) {
    if (!keys_down_.contains(key)) {
      keys_pressed_.insert(key);
    }
    keys_down_.insert(key);
  }

  void set_key_up(KeyCode key) {
    if (keys_down_.contains(key)) {
      keys_released_.insert(key);
    }
    keys_down_.erase(key);
  }

  void set_mouse_delta(f32 dx, f32 dy) {
    mouse_.delta_x = dx;
    mouse_.delta_y = dy;
  }

  std::unordered_set<KeyCode> &keys_down() { return keys_down_; }

private:
  std::unordered_set<KeyCode> keys_down_;
  std::unordered_set<KeyCode> keys_pressed_;
  std::unordered_set<KeyCode> keys_released_;
  MouseState mouse_;
};

inline void input_polling_system(ResMut<Input> input, NonSendMarker) {
  input->clear_frame();

  // Drain raylib's key pressed queue
  i32 key;
  while ((key = rl::GetKeyPressed()) != 0) {
    input->set_key_down(key);
  }

  // Check releases for currently tracked keys
  auto &down = input->keys_down();
  std::vector<KeyCode> released;
  for (KeyCode k : down) {
    if (rl::IsKeyUp(k)) {
      released.push_back(k);
    }
  }
  for (KeyCode k : released) {
    input->set_key_up(k);
  }

  rl::Vector2 md = rl::GetMouseDelta();
  input->set_mouse_delta(md.x, md.y);
}

inline void input_plugin(App &app) {
  app.insert_resource(Input{});
  app.add_systems(ScheduleLabel::First, input_polling_system);
}

} // namespace ecs
