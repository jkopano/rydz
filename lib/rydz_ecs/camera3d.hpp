#pragma once
#include <raylib-cpp/raylib-cpp.hpp>

namespace ecs {

struct Camera3DComponent {
  float fov = 45.0f;
  int projection = CAMERA_PERSPECTIVE;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;
};

struct ActiveCamera {};

inline Camera3D build_camera(Vector3 position, Vector3 forward, Vector3 up,
                             const Camera3DComponent &comp) {
  Camera3D cam = {};
  cam.position = position;
  cam.target = Vector3Add(position, forward);
  cam.up = up;
  cam.fovy = comp.fov;
  cam.projection = comp.projection;
  return cam;
}

} // namespace ecs
