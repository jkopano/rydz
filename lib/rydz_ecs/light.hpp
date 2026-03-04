#pragma once
#include <raylib.h>
#include <raymath.h>

namespace ecs {

struct PointLight {
  Color color = WHITE;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  Color color = WHITE;
  Vector3 direction = {-1, -1, -1};
  float intensity = 1.0f;
};

struct AmbientLight {
  Color color = {40, 40, 40, 255};
  float intensity = 0.3f;
};

} // namespace ecs
