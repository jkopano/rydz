#pragma once
#include "math.hpp"
#include "rydz_ecs/requires.hpp"
#include "rydz_graphics/color.hpp"
#include "rydz_graphics/transform.hpp"
#include <algorithm>

namespace ecs {

using namespace math;

enum class CameraProjection3D {
  Perspective,
  Orthographic,
};

struct Camera3DComponent {
  using Required = Requires<ClearColor>;
  CameraProjection3D projection = CameraProjection3D::Perspective;
  float perspective_fov_y_deg = 45.0f;
  float orthographic_height = 10.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;

  static constexpr Camera3DComponent perspective(float fov_y_deg = 45.0f,
                                                 float near_plane = 0.1f,
                                                 float far_plane = 1000.0f) {
    return {
        .projection = CameraProjection3D::Perspective,
        .perspective_fov_y_deg = fov_y_deg,
        .orthographic_height = 10.0f,
        .near_plane = near_plane,
        .far_plane = far_plane,
    };
  }

  static constexpr Camera3DComponent orthographic(float height = 220.0f,
                                                  float near_plane = 0.1f,
                                                  float far_plane = 1000.0f) {
    return {
        .projection = CameraProjection3D::Orthographic,
        .perspective_fov_y_deg = 45.0f,
        .orthographic_height = height,
        .near_plane = near_plane,
        .far_plane = far_plane,
    };
  }

  [[nodiscard]] constexpr bool is_perspective() const {
    return projection == CameraProjection3D::Perspective;
  }

  [[nodiscard]] constexpr bool is_orthographic() const {
    return projection == CameraProjection3D::Orthographic;
  }
};

struct ActiveCamera {};

struct CameraView {
  Mat4 view;
  Mat4 proj;
  Vec3 position;
};

inline float compute_camera_aspect_ratio(float width, float height) {
  float aspect = width / std::max(height, 1.0f);
  return std::max(aspect, 0.0001f);
}

inline Mat4 compute_camera_projection(const Camera3DComponent &comp,
                                      float aspect) {
  if (comp.is_orthographic()) {
    float height = std::max(comp.orthographic_height, 0.0001f);
    float half_height = height * 0.5f;
    float half_width = half_height * aspect;
    return Mat4::sOrthographic(-half_width, half_width, -half_height,
                               half_height, comp.near_plane, comp.far_plane);
  }

  float fov_rad = comp.perspective_fov_y_deg * DEG2RAD;
  return Mat4::sPerspective(fov_rad, aspect, comp.near_plane, comp.far_plane);
}

inline CameraView compute_camera_view(Vec3 position, Vec3 forward, Vec3 up,
                                      const Camera3DComponent &comp,
                                      float aspect) {
  Mat4 view = Mat4::sLookAt(position, position + forward, up);
  Mat4 proj = compute_camera_projection(comp, aspect);
  return {view, proj, position};
}

inline CameraView compute_camera_view(const Transform &t,
                                      const Camera3DComponent &comp,
                                      float aspect) {
  return compute_camera_view(t.translation, t.forward(), t.up(), comp, aspect);
}

inline CameraView compute_camera_view(const GlobalTransform &t,
                                      const Camera3DComponent &comp,
                                      float aspect) {
  return compute_camera_view(t.translation(), t.forward(), t.up(), comp,
                             aspect);
}

} // namespace ecs
