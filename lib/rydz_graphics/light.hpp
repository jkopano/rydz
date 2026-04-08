#pragma once

#include "math.hpp"
#include "rydz_gl/core.hpp"

namespace ecs {

using namespace math;

struct PointLight {
  rydz_gl::Color color = rydz_gl::kWhite;
  float intensity = 1.0f;
  float range = 10.0f;
};

struct DirectionalLight {
  rydz_gl::Color color = rydz_gl::kWhite;
  Vec3 direction = Vec3(-1, -1, -1);
  float intensity = 1.0f;
};

struct AmbientLight {
  rydz_gl::Color color = {40, 40, 40, 255};
  float intensity = 0.3f;
};

} // namespace ecs
