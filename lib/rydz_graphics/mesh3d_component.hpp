#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/requires.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_graphics/visibility.hpp"

namespace ecs {

using gl::Mesh;

struct Mesh3d {
  using Required = Requires<Visibility, Transform>;
  Handle<Mesh> mesh;

  Mesh3d() = default;
  explicit Mesh3d(Handle<Mesh> h) : mesh(h) {}
};

} // namespace ecs
