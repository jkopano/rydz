#pragma once

#include "components.hpp"
#include "math.hpp"

// ui marker
struct UiNode {
  int z_index = 0;
};

struct ComputedUiNode {
  math::Vec2 pos;
  math::Vec2 size;
  math::Vec2 content_size;
};
