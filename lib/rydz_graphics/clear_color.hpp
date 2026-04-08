#pragma once

#include "rydz_ecs/fwd.hpp"
#include "rydz_gl/core.hpp"

namespace ecs {

struct ClearColor {
  using T = Component;
  rydz_gl::Color color = {30, 30, 40, 255};
};

} // namespace ecs
