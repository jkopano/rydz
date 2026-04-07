#pragma once

#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"

namespace ecs {

struct ClearColor {
  using T = Component;
  rl::Color color = {30, 30, 40, 255};
};

} // namespace ecs
