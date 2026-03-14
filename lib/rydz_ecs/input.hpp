#pragma once
#include "app.hpp"
#include <unordered_set>

namespace ecs {

using KeyCode = int;

struct MouseState {
  float delta_x = 0.0f;
  float delta_y = 0.0f;
};

struct Input {
  using Type = ResourceType;
  bool key_down(KeyCode key) const { return keys_down_.contains(key); }
  bool key_pressed(KeyCode key) const { return keys_pressed_.contains(key); }
  bool key_released(KeyCode key) const { return keys_released_.contains(key); }

  Vector2 mouse_delta() const {
    return Vector2{mouse_.delta_x, mouse_.delta_y};
  }
  float mouse_delta_x() const { return mouse_.delta_x; }
  float mouse_delta_y() const { return mouse_.delta_y; }

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

  void set_mouse_delta(float dx, float dy) {
    mouse_.delta_x = dx;
    mouse_.delta_y = dy;
  }

private:
  std::unordered_set<KeyCode> keys_down_;
  std::unordered_set<KeyCode> keys_pressed_;
  std::unordered_set<KeyCode> keys_released_;
  MouseState mouse_;
};

inline constexpr KeyCode POLLED_KEYS[] = {
    // Letters A-Z (65-90)
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    // Digits 0-9 (48-57)
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    // Function keys F1-F12 (290-301)
    290,
    291,
    292,
    293,
    294,
    295,
    296,
    297,
    298,
    299,
    300,
    301,
    // Special keys
    32,  // SPACE
    256, // ESCAPE
    257, // ENTER
    258, // TAB
    259, // BACKSPACE
    261, // DELETE
    262, // RIGHT
    263, // LEFT
    264, // DOWN
    265, // UP
    340, // LEFT_SHIFT
    341, // LEFT_CONTROL
    342, // LEFT_ALT
    344, // RIGHT_SHIFT
    345, // RIGHT_CONTROL
    346, // RIGHT_ALT
};

inline void input_polling_system(ResMut<Input> input, NonSendMarker) {
  input->clear_frame();

  for (KeyCode key : POLLED_KEYS) {
    if (rl::IsKeyDown(key)) {
      input->set_key_down(key);
    } else {
      input->set_key_up(key);
    }
  }

  rl::Vector2 md = rl::GetMouseDelta();
  input->set_mouse_delta(md.x, md.y);
}

inline void input_plugin(App &app) {
  app.insert_resource(Input{});
  app.add_systems(ScheduleLabel::First, input_polling_system);
}

} // namespace ecs
