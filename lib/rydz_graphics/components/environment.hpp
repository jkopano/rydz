#pragma once

#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <string>
#include <utility>

namespace ecs {

struct Environment {
  using T = Component;

  Color clear_color = ClearColor{}.color;
  gl::CubeMap skybox{};

  Environment() = default;
  Environment(Environment const&) = delete;
  auto operator=(Environment const&) -> Environment& = delete;
  Environment(Environment&&) noexcept = default;
  auto operator=(Environment&&) noexcept -> Environment& = default;

  [[nodiscard]] auto has_skybox() const -> bool { return skybox.ready(); }

  static auto from_color(Color clear_color) -> Environment {
    Environment environment{};
    environment.clear_color = clear_color;
    return environment;
  }

  static auto from_cubemap(
    gl::CubeMap cubemap, Color clear_color = ClearColor{}.color
  ) -> Environment {
    Environment environment{};
    environment.clear_color = clear_color;
    environment.skybox = std::move(cubemap);
    return environment;
  }

  static auto from_directory(
    std::string const& dir,
    std::string const& ext = ".jpg",
    Color clear_color = ClearColor{}.color
  ) -> Environment {
    return from_cubemap(gl::CubeMap::from_directory(dir, ext), clear_color);
  }
};

} // namespace ecs
