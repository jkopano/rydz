#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include "types.hpp"
#include <unordered_set>
#include <vector>

namespace ecs {

using KeyCode = i32;

struct MouseState {
  f32 delta_x = 0.0f;
  f32 delta_y = 0.0f;
  f32 x = 0.0f;
  f32 y = 0.0f;
};

struct Input {
  using T = Resource;
  auto key_down(KeyCode key) const -> bool { return keys_down_.contains(key); }
  auto key_pressed(KeyCode key) const -> bool { return keys_pressed_.contains(key); }
  auto key_released(KeyCode key) const -> bool { return keys_released_.contains(key); }

  auto mouse_delta() const -> math::Vec2 {
    return math::Vec2{mouse_.delta_x, mouse_.delta_y};
  }
  auto mouse_delta_x() const -> f32 { return mouse_.delta_x; }
  auto mouse_delta_y() const -> f32 { return mouse_.delta_y; }

public:
  auto clear_frame() -> void {
    keys_pressed_.clear();
    keys_released_.clear();
    mouse_.delta_x = 0.0f;
    mouse_.delta_y = 0.0f;
  }

  auto set_key_down(KeyCode key) -> void {
    if (!keys_down_.contains(key)) {
      keys_pressed_.insert(key);
    }
    keys_down_.insert(key);
  }

  auto set_key_up(KeyCode key) -> void {
    if (keys_down_.contains(key)) {
      keys_released_.insert(key);
    }
    keys_down_.erase(key);
  }

  auto set_mouse_delta(f32 dx, f32 dy) -> void {
    mouse_.delta_x = dx;
    mouse_.delta_y = dy;
  }

  auto set_mouse_position(f32 x, f32 y) -> void {
    mouse_.x = x;
    mouse_.y = y;
  }

  auto keys_down() -> std::unordered_set<KeyCode>& { return keys_down_; }

  static auto input_polling_system(ResMut<Input> input, NonSendMarker) -> void {
    input->clear_frame();

    i32 key;
    while ((key = rl::GetKeyPressed()) != 0) {
      input->set_key_down(key);
    }

    auto& down = input->keys_down();
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

    rl::Vector2 mp = rl::GetMousePosition();
    input->set_mouse_position(mp.x, mp.y);
  }

private:
  std::unordered_set<KeyCode> keys_down_;
  std::unordered_set<KeyCode> keys_pressed_;
  std::unordered_set<KeyCode> keys_released_;
  MouseState mouse_;
};

struct InputPlugin : ecs::IPlugin {
  auto build(App& app) const -> void {
    app.insert_resource(Input{});
    app.add_systems(First, Input::input_polling_system);
  }
};

} // namespace ecs
