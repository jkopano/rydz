#pragma once
#include "math.hpp"
#include "rl.hpp"

namespace ecs {

using namespace math;

struct PointLight {
  rl::Color color = WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  rl::Color color = WHITE;
  Vec3 direction = Vec3(-1, -1, -1);
  float intensity = 1.0f;
};

struct AmbientLight {
  rl::Color color = {40, 40, 40, 255};
  float intensity = 0.3f;
};

} // namespace ecs
