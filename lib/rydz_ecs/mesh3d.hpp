#pragma once
#include "asset.hpp"
#include <raylib-cpp/raylib-cpp.hpp>

namespace ecs {

struct Mesh3d {
  Handle<Model> model;

  Mesh3d() = default;
  explicit Mesh3d(Handle<Model> h) : model(h) {}
};

namespace mesh {

inline Model cube(float width = 1, float height = 1, float length = 1) {
  Mesh m = GenMeshCube(width, height, length);
  return LoadModelFromMesh(m);
}

inline Model sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  Mesh m = GenMeshSphere(radius, rings, slices);
  return LoadModelFromMesh(m);
}

inline Model plane(float width = 10, float length = 10, int res_x = 1,
                   int res_z = 1) {
  Mesh m = GenMeshPlane(width, length, res_x, res_z);
  return LoadModelFromMesh(m);
}

inline Model cylinder(float radius = 0.5f, float height = 1, int slices = 16) {
  Mesh m = GenMeshCylinder(radius, height, slices);
  return LoadModelFromMesh(m);
}

inline Model torus(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                   int sides = 16) {
  Mesh m = GenMeshTorus(radius, size, rad_seg, sides);
  return LoadModelFromMesh(m);
}

inline Model hemisphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  Mesh m = GenMeshHemiSphere(radius, rings, slices);
  return LoadModelFromMesh(m);
}

inline Model knot(float radius = 0.5f, float size = 0.2f, int rad_seg = 16,
                  int sides = 16) {
  Mesh m = GenMeshKnot(radius, size, rad_seg, sides);
  return LoadModelFromMesh(m);
}

} // namespace mesh

} // namespace ecs
