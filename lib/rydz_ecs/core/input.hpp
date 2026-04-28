#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include "types.hpp"
#include <unordered_set>
#include <vector>

namespace ecs {

using KeyCode = i32;
using MouseButton = i32;

struct MouseState {
  f32 delta_x = 0.0f;
  f32 delta_y = 0.0f;
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 scroll = 0.0f;
};

struct Input {
  using T = Resource;

  auto key_down(KeyCode const key) const -> bool { return keys_down_.contains(key); }
  auto key_pressed(KeyCode const key) const -> bool {
    return keys_pressed_.contains(key);
  }
  auto key_released(KeyCode const key) const -> bool {
    return keys_released_.contains(key);
  }

  auto mouse_down(MouseButton const button) const -> bool {
    return mouse_down_.contains(button);
  }
  auto mouse_pressed(MouseButton const button) const -> bool {
    return mouse_pressed_.contains(button);
  }
  auto mouse_released(MouseButton const button) const -> bool {
    return mouse_released_.contains(button);
  }

  auto mouse_position() const -> math::Vec2 { return math::Vec2{mouse_.x, mouse_.y}; }
  auto mouse_x() const -> f32 { return mouse_.x; }
  auto mouse_y() const -> f32 { return mouse_.y; }

  auto mouse_delta() const -> math::Vec2 {
    return math::Vec2{mouse_.delta_x, mouse_.delta_y};
  }
  auto mouse_delta_x() const -> f32 { return mouse_.delta_x; }
  auto mouse_delta_y() const -> f32 { return mouse_.delta_y; }
  auto mouse_scroll() const -> f32 { return mouse_.scroll; }

  static auto input_polling_system(ResMut<Input> input, NonSendMarker) -> void {
    input->clear_frame();

    i32 key{};
    while ((key = rl::GetKeyPressed()) != 0) {
      input->set_key_down(key);
    }

    std::vector<KeyCode> keys_to_release;
    for (KeyCode k : input->keys_down()) {
      if (rl::IsKeyUp(k)) {
        keys_to_release.push_back(k);
      }
    }
    for (KeyCode k : keys_to_release) {
      input->set_key_up(k);
    }

    for (int b = 0; b < 7; b++) {
      if (rl::IsMouseButtonPressed(b)) {
        input->set_mouse_button_down(b);
      }
      if (rl::IsMouseButtonReleased(b)) {
        input->set_mouse_button_up(b);
      }
    }

    auto md = rl::GetMouseDelta();
    input->set_mouse_delta(md.x, md.y);

    auto mp = rl::GetMousePosition();
    input->set_mouse_position(mp.x, mp.y);

    input->set_mouse_scroll(rl::GetMouseWheelMove());
  }

  auto clear() -> void {
    keys_down_.clear();
    keys_pressed_.clear();
    keys_released_.clear();
    mouse_.delta_x = 0.0f;
    mouse_.delta_y = 0.0f;
  }

private:
  std::unordered_set<KeyCode> keys_down_;
  std::unordered_set<KeyCode> keys_pressed_;
  std::unordered_set<KeyCode> keys_released_;

  std::unordered_set<MouseButton> mouse_down_;
  std::unordered_set<MouseButton> mouse_pressed_;
  std::unordered_set<MouseButton> mouse_released_;

  MouseState mouse_;

  auto set_key_down(KeyCode const key) -> void {
    if (!keys_down_.contains(key)) {
      keys_pressed_.insert(key);
    }
    keys_down_.insert(key);
  }
  auto set_key_up(KeyCode const key) -> void {
    if (keys_down_.contains(key)) {
      keys_released_.insert(key);
    }
    keys_down_.erase(key);
  }

  auto set_mouse_button_down(MouseButton const button) -> void {
    if (!mouse_down_.contains(button)) {
      mouse_pressed_.insert(button);
    }
    mouse_down_.insert(button);
  }
  auto set_mouse_button_up(MouseButton const button) -> void {
    if (mouse_down_.contains(button)) {
      mouse_released_.insert(button);
    }
    mouse_down_.erase(button);
  }

  auto clear_frame() -> void {
    keys_pressed_.clear();
    keys_released_.clear();
    mouse_pressed_.clear();
    mouse_released_.clear();

    mouse_.delta_x = 0.0f;
    mouse_.delta_y = 0.0f;
    mouse_.scroll = 0.0f;
  }

  auto set_mouse_delta(f32 dx, f32 dy) -> void {
    mouse_.delta_x = dx;
    mouse_.delta_y = dy;
  }
  auto set_mouse_position(f32 x, f32 y) -> void {
    mouse_.x = x;
    mouse_.y = y;
  }
  auto set_mouse_scroll(f32 s) -> void { mouse_.scroll = s; }
  auto keys_down() -> std::unordered_set<KeyCode>& { return keys_down_; }
  auto mouse_down_set() -> std::unordered_set<MouseButton>& { return mouse_down_; }
};

struct InputPlugin : ecs::IPlugin {
  auto build(App& app) const -> void {
    app.insert_resource(Input{});
    app.add_systems(First, Input::input_polling_system);
  }
};

} // namespace ecs
