#pragma once

#include "math.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/fwd.hpp"

namespace rydz::ui {

struct UiHoverEnter {
  using T = ecs::EntityEvent;
  ecs::Entity target;
  math::Vec2 cursor;
};

struct UiHoverLeave {
  using T = ecs::EntityEvent;
  ecs::Entity target;
  math::Vec2 cursor;
};

struct UiClick {
  using T = ecs::EntityEvent;
  ecs::Entity target;
  math::Vec2 cursor;
  int button;
};

struct UiFocusGained {
  using T = ecs::EntityEvent;
  ecs::Entity target;
};

struct UiFocusLost {
  using T = ecs::EntityEvent;
  ecs::Entity target;
};

} // namespace rydz::ui
