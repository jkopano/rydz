#pragma once

#include "rydz_gl/resources.hpp"

namespace ecs {

struct Texture {
  rydz_gl::Texture value{};

  Texture() = default;
  Texture(rydz_gl::Texture texture) : value(texture) {}

  operator rydz_gl::Texture &() { return value; }
  operator const rydz_gl::Texture &() const { return value; }
};

struct Mesh {
  rydz_gl::Mesh value{};

  Mesh() = default;
  Mesh(rydz_gl::Mesh mesh) : value(mesh) {}

  operator rydz_gl::Mesh &() { return value; }
  operator const rydz_gl::Mesh &() const { return value; }
};

struct Sound {
  rydz_gl::Sound value{};

  Sound() = default;
  Sound(rydz_gl::Sound sound) : value(sound) {}

  operator rydz_gl::Sound &() { return value; }
  operator const rydz_gl::Sound &() const { return value; }
};

} // namespace ecs
