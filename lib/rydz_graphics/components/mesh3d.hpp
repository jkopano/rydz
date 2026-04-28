#pragma once

#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include "rydz_graphics/spatial/visibility.hpp"

namespace ecs {

using gl::Mesh;

struct Mesh3d {
  using Required = Requires<Visibility, Transform>;
  Handle<Mesh> mesh;

  Mesh3d() = default;
  explicit Mesh3d(Handle<Mesh> h) : mesh(h) {}
};

} // namespace ecs
