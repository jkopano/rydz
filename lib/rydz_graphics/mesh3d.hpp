#pragma once
#include "rydz_ecs/asset.hpp"
#include <raylib.h>
#include <raymath.h>

namespace ecs {

struct Mesh3d {
  Handle<Model> model;

  Mesh3d() = default;
  explicit Mesh3d(Handle<Model> h) : model(h) {}
};

namespace mesh {

inline void ensure_uploaded(Mesh &m) {
  if (m.vaoId == 0 && m.vboId != nullptr) {
    RL_FREE(m.vboId);
    m.vboId = nullptr;
    UploadMesh(&m, false);
  }
}

inline Model cube(float width = 1, float height = 1, float length = 1) {
  Mesh m = GenMeshCube(width, height, length);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  Mesh m = GenMeshSphere(radius, rings, slices);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model plane(float width = 10, float length = 10, int res_x = 1,
                   int res_z = 1) {
  Mesh m = GenMeshPlane(width, length, res_x, res_z);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model cylinder(float radius = 0.5f, float height = 1, int slices = 16) {
  Mesh m = GenMeshCylinder(radius, height, slices);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model torus(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                   int sides = 16) {
  Mesh m = GenMeshTorus(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model hemisphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  Mesh m = GenMeshHemiSphere(radius, rings, slices);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

inline Model knot(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                  int sides = 16) {
  Mesh m = GenMeshKnot(radius, size, rad_seg, sides);
  ensure_uploaded(m);
  return LoadModelFromMesh(m);
}

} // namespace mesh

} // namespace ecs
