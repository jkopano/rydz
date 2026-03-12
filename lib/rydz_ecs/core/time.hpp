#pragma once
#include "rydz_ecs/system.hpp"

namespace ecs {

struct Time {
  float delta_seconds = 0.0f;
  float elapsed_seconds = 0.0f;
  uint64_t frame_count = 0;
};

} // namespace ecs
