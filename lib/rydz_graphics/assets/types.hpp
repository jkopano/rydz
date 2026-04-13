#pragma once

#include "rydz_graphics/gl/resources.hpp"

namespace ecs {

struct Texture {
  gl::Texture value{};

  Texture() = default;
  Texture(gl::Texture texture) : value(texture) {}

  operator gl::Texture &() { return value; }
  operator const gl::Texture &() const { return value; }
};

struct Mesh {
  gl::Mesh value{};

  Mesh() = default;
  Mesh(gl::Mesh mesh) : value(mesh) {}

  operator gl::Mesh &() { return value; }
  operator const gl::Mesh &() const { return value; }
};

struct Sound {
  gl::Sound value{};

  Sound() = default;
  Sound(gl::Sound sound) : value(sound) {}

  operator gl::Sound &() { return value; }
  operator const gl::Sound &() const { return value; }
};

} // namespace ecs
