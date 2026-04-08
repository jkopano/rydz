#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/requires.hpp"
#include "rydz_gl/resources.hpp"
#include "rydz_graphics/material3d.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"

namespace ecs {

struct Mesh3d {
  using Required = Requires<Visibility, Transform>;
  Handle<rydz_gl::Mesh> mesh;

  Mesh3d() = default;
  explicit Mesh3d(Handle<rydz_gl::Mesh> h) : mesh(h) {}
};

namespace mesh {

inline void ensure_uploaded(rydz_gl::Mesh &mesh) {
  mesh.gen_tangents();
  rydz_gl::upload_mesh(mesh, false);
}

inline rydz_gl::Mesh cube(float width = 1, float height = 1, float length = 1) {
  auto mesh = rydz_gl::gen_cube(width, height, length);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh sphere(float radius = 0.5f, int rings = 16,
                            int slices = 16) {
  auto mesh = rydz_gl::gen_sphere(radius, rings, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh plane(float width = 10, float length = 10, int res_x = 1,
                           int res_z = 1) {
  auto mesh = rydz_gl::gen_plane(width, length, res_x, res_z);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh cylinder(float radius = 0.5f, float height = 1,
                              int slices = 16) {
  auto mesh = rydz_gl::gen_cylinder(radius, height, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh torus(float radius = 0.5f, float size = 0.2f,
                           int rad_seg = 16, int sides = 16) {
  auto mesh = rydz_gl::gen_torus(radius, size, rad_seg, sides);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh hemisphere(float radius = 0.5f, int rings = 16,
                                int slices = 16) {
  auto mesh = rydz_gl::gen_hemisphere(radius, rings, slices);
  ensure_uploaded(mesh);
  return mesh;
}

inline rydz_gl::Mesh knot(float radius = 0.5f, float size = 0.2f,
                          int rad_seg = 16, int sides = 16) {
  auto mesh = rydz_gl::gen_knot(radius, size, rad_seg, sides);
  ensure_uploaded(mesh);
  return mesh;
}

} // namespace mesh

} // namespace ecs
