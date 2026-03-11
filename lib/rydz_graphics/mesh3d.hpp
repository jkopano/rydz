#pragma once
#include "rydz_ecs/asset.hpp"
#include "rl.hpp"

namespace ecs {

struct Mesh3d {
  Handle<rl::Model> model;

  Mesh3d() = default;
  explicit Mesh3d(Handle<rl::Model> h) : model(h) {}
};

namespace mesh {

inline void ensure_uploaded(rl::Mesh &m) {
  if (m.vaoId == 0 && m.vboId != nullptr) {
    RL_FREE(m.vboId);
    m.vboId = nullptr;
    rl::UploadMesh(&m, false);
  }
}

inline rl::Model cube(float width = 1, float height = 1, float length = 1) {
  rl::Mesh m = rl::GenMeshCube(width, height, length);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  rl::Mesh m = rl::GenMeshSphere(radius, rings, slices);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model plane(float width = 10, float length = 10, int res_x = 1,
                   int res_z = 1) {
  rl::Mesh m = rl::GenMeshPlane(width, length, res_x, res_z);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model cylinder(float radius = 0.5f, float height = 1, int slices = 16) {
  rl::Mesh m = rl::GenMeshCylinder(radius, height, slices);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model torus(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                   int sides = 16) {
  rl::Mesh m = rl::GenMeshTorus(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model hemisphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  rl::Mesh m = rl::GenMeshHemiSphere(radius, rings, slices);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

inline rl::Model knot(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                  int sides = 16) {
  rl::Mesh m = rl::GenMeshKnot(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return rl::LoadModelFromMesh(m);
}

} // namespace mesh

} // namespace ecs
