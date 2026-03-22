#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "rydz_graphics/transform.hpp"

namespace ecs {

using namespace math;

struct Camera3DComponent {
  double fov = 45.0;
  int projection = CAMERA_PERSPECTIVE;
  double near_plane = 0.1;
  double far_plane = 1000.0;
};

struct ActiveCamera {};

struct CameraView {
  Mat4 view;
  Mat4 proj;
  Vec3 position;
};

inline CameraView compute_camera_view(const Transform &t,
                                      const Camera3DComponent &comp) {
  float aspect = static_cast<float>(rl::GetScreenWidth()) /
                 static_cast<float>(std::max(rl::GetScreenHeight(), 1));
  float fov_rad = static_cast<float>(comp.fov) * DEG2RAD;

  Mat4 view = Mat4::sLookAt(t.translation, t.translation + t.forward(), t.up());
  Mat4 proj =
      Mat4::sPerspective(fov_rad, aspect, static_cast<float>(comp.near_plane),
                         static_cast<float>(comp.far_plane));
  return {view, proj, t.translation};
}

} // namespace ecs
