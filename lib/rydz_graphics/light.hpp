#pragma once

#include "math.hpp"
#include "rydz_graphics/types.hpp"

namespace ecs {

using namespace math;

struct PointLight {
  Color color = kWhite;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  Color color = kWhite;
  Vec3 direction = Vec3(-1, -1, -1);
  float intensity = 1.0f;
};

struct AmbientLight {
  Color color = {40, 40, 40, 255};
  float intensity = 0.3f;
};

} // namespace ecs
