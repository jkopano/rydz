#pragma once

#include "math.hpp"
#include "rydz_graphics/color.hpp"

namespace ecs {

using namespace math;

struct PointLight {
  Color color = Color::WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  Color color = Color::WHITE;
  Vec3 direction = Vec3(-1, -1, -1);
  float intensity = 1.0f;
};

struct AmbientLight {
  Color color = Color::RAYWHITE;
  float intensity = 0.3f;
};

} // namespace ecs
