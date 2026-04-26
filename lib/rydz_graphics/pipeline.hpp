#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/core.hpp"
#include <algorithm>

namespace ecs {

struct DebugOverlaySettings {
  using T = Resource;

  bool draw_fps = true;
  Vec2 fps_position = {10.0f, 10.0f};
};

} // namespace ecs
