#pragma once

#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/mesh3d_component.hpp"

namespace ecs {

namespace mesh {

inline auto ensure_uploaded(Mesh& mesh) -> void {
  mesh.gen_tangents();
  mesh.upload(false);
}

inline auto cube(float width = 1, float height = 1, float length = 1) -> Mesh {
  auto mesh = gl::gen_cube(width, height, length);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto sphere(float radius = 0.5f, int rings = 16, int slices = 16)
  -> Mesh {
  auto mesh = gl::gen_sphere(radius, rings, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto plane(
  float width = 10, float length = 10, int res_x = 1, int res_z = 1
) -> Mesh {
  auto mesh = gl::gen_plane(width, length, res_x, res_z);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto cylinder(float radius = 0.5f, float height = 1, int slices = 16)
  -> Mesh {
  auto mesh = gl::gen_cylinder(radius, height, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto torus(
  float radius = 0.5f, float size = 0.2f, int rad_seg = 16, int sides = 16
) -> Mesh {
  auto mesh = gl::gen_torus(radius, size, rad_seg, sides);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto hemisphere(float radius = 0.5f, int rings = 16, int slices = 16)
  -> Mesh {
  auto mesh = gl::gen_hemisphere(radius, rings, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline auto knot(
  float radius = 0.5f, float size = 0.2f, int rad_seg = 16, int sides = 16
) -> Mesh {
  auto mesh = gl::gen_knot(radius, size, rad_seg, sides);
  ensure_uploaded(mesh);
  return mesh;
}

} // namespace mesh

} // namespace ecs
