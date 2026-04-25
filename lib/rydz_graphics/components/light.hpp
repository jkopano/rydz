#pragma once

#include "math.hpp"
#include "rydz_graphics/components/color.hpp"

namespace ecs {

using namespace math;

struct PointLight {
  Color color = Color::WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
  bool casts_shadows = false;
};

struct SpotLight {
  Color color = Color::WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
  float inner_angle = 20.0f * DEG2RAD;
  float outer_angle = 30.0f * DEG2RAD;
  bool casts_shadows = false;
};

struct DirectionalLight {
  Color color = Color::WHITE;
  Vec3 direction = Vec3(-1, -1, -1);
  float intensity = 1.0f;
  bool casts_shadows = true;
};

struct AmbientLight {
  Color color = Color::RAYWHITE;
  float intensity = 0.1f;
};

} // namespace ecs
