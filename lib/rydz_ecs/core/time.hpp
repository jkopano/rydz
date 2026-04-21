#pragma once
#include "rydz_ecs/fwd.hpp"

namespace ecs {

struct Time {
  using T = Resource;
  float delta_seconds = 0.0f;
  float elapsed_seconds = 0.0f;
  uint64_t frame_count = 0;
};

struct FixedTime {
  using T = Resource;
  float hz = 60.0f;
  float accumulator = 0.0f;
  float overstep = 0.0f;

  [[nodiscard]] auto step() const -> float { return 1.0f / hz; }
};

} // namespace ecs
