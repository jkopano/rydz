#pragma once
#include "raylib.h"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/requires.hpp"
#include "rydz_graphics/material3d.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"
#include <cstring>

namespace ecs {

struct Mesh3d {
  using Required = Requires<Visibility, Material3d, Transform>;
  Handle<rl::Mesh> mesh;

  Mesh3d() = default;
  explicit Mesh3d(Handle<rl::Mesh> h) : mesh(h) {}
};

namespace mesh {

inline void ensure_tangents(rl::Mesh &m) { rl::GenMeshTangents(&m); }

inline void ensure_uploaded(rl::Mesh &m) {
  ensure_tangents(m);
  rl::UploadMesh(&m, false);
}

inline rl::Mesh cube(float width = 1, float height = 1, float length = 1) {
  rl::Mesh m = rl::GenMeshCube(width, height, length);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  rl::Mesh m = rl::GenMeshSphere(radius, rings, slices);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh plane(float width = 10, float length = 10, int res_x = 1,
                      int res_z = 1) {
  rl::Mesh m = rl::GenMeshPlane(width, length, res_x, res_z);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh cylinder(float radius = 0.5f, float height = 1,
                         int slices = 16) {
  rl::Mesh m = rl::GenMeshCylinder(radius, height, slices);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh torus(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                      int sides = 16) {
  rl::Mesh m = rl::GenMeshTorus(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh hemisphere(float radius = 0.5f, int rings = 16,
                           int slices = 16) {
  rl::Mesh m = rl::GenMeshHemiSphere(radius, rings, slices);
  ensure_uploaded(m);
  return m;
}

inline rl::Mesh knot(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                     int sides = 16) {
  rl::Mesh m = rl::GenMeshKnot(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return m;
}

} // namespace mesh

} // namespace ecs
