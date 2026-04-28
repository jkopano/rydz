#pragma once
#include "rydz_ecs/storage.hpp"
#include "types.hpp"

namespace ecs {

struct Player {
  using Storage = SparseSetStorage<Player>;
  f32 move_speed = 8.0f;
};

} // namespace ecs
