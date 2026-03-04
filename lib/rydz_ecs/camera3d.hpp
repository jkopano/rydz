#pragma once
#include <raylib.h>
#include <raymath.h>

namespace ecs {

struct Camera3DComponent {
  double fov = 45.0;
  int projection = CAMERA_PERSPECTIVE;
  double near_plane = 0.1;
  double far_plane = 1000.0;
};

struct ActiveCamera {};

inline Camera3D build_camera(Vector3 position, Vector3 forward, Vector3 up,
                             const Camera3DComponent &comp) {
  Camera3D cam = {};
  cam.position = position;
  cam.target = Vector3Add(position, forward);
  cam.up = up;
  cam.fovy = static_cast<float>(comp.fov);
  cam.projection = comp.projection;
  return cam;
}

} // namespace ecs
