#pragma once
#include "rl.hpp"

namespace ecs {

struct PointLight {
  rl::Color color = WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  rl::Color color = WHITE;
  rl::Vector3 direction = {-1, -1, -1};
  float intensity = 1.0f;
};

struct AmbientLight {
  rl::Color color = {40, 40, 40, 255};
  float intensity = 0.3f;
};

} // namespace ecs
