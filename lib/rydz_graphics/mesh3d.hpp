#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/requires.hpp"
#include "rydz_graphics/assets/types.hpp"
#include "rydz_graphics/material3d.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"

namespace ecs {

struct Mesh3d {
  using Required = Requires<Visibility, Transform>;
  Handle<Mesh> mesh;

  Mesh3d() = default;
  explicit Mesh3d(Handle<Mesh> h) : mesh(h) {}
};

namespace mesh {

inline void ensure_uploaded(Mesh &mesh) {
  mesh.value.gen_tangents();
  gl::upload_mesh(mesh.value, false);
}

inline Mesh cube(float width = 1, float height = 1, float length = 1) {
  auto mesh = gl::gen_cube(width, height, length);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  auto mesh = gl::gen_sphere(radius, rings, slices);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh plane(float width = 10, float length = 10, int res_x = 1,
                  int res_z = 1) {
  auto mesh = gl::gen_plane(width, length, res_x, res_z);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh cylinder(float radius = 0.5f, float height = 1, int slices = 16) {
  auto mesh = gl::gen_cylinder(radius, height, slices);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh torus(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                  int sides = 16) {
  auto mesh = gl::gen_torus(radius, size, rad_seg, sides);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh hemisphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  auto mesh = gl::gen_hemisphere(radius, rings, slices);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

inline Mesh knot(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                 int sides = 16) {
  auto mesh = gl::gen_knot(radius, size, rad_seg, sides);
  Mesh asset{mesh};
  ensure_uploaded(asset);
  return asset;
}

} // namespace mesh

} // namespace ecs
