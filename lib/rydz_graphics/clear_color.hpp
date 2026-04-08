#pragma once

#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/types.hpp"

namespace ecs {

struct ClearColor {
  using T = Component;
  Color color = {30, 30, 40, 255};
};

} // namespace ecs
