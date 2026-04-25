#pragma once

#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/components/light.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <string>
#include <utility>

namespace ecs {

struct Environment {
  using T = Component;

  Color clear_color = ClearColor{}.color;
  AmbientLight ambient_light{};
  DirectionalLight directional_light{
    .color = Color::WHITE,
    .direction = Vec3(-1.0F, -1.0F, -1.0F),
    .intensity = 0.3F,
    .casts_shadows = true,
  };
  gl::CubeMap skybox{};

  Environment() = default;
  Environment(Environment const&) = delete;
  auto operator=(Environment const&) -> Environment& = delete;
  Environment(Environment&&) noexcept = default;
  auto operator=(Environment&&) noexcept -> Environment& = default;

  [[nodiscard]] auto has_skybox() const -> bool { return skybox.ready(); }

  auto with_ambient_light(AmbientLight light) & -> Environment& {
    ambient_light = light;
    return *this;
  }

  auto with_ambient_light(AmbientLight light) && -> Environment&& {
    ambient_light = light;
    return std::move(*this);
  }

  auto with_directional_light(DirectionalLight light) & -> Environment& {
    directional_light = light;
    return *this;
  }

  auto with_directional_light(DirectionalLight light) && -> Environment&& {
    directional_light = light;
    return std::move(*this);
  }

  auto with_lighting(AmbientLight ambient, DirectionalLight directional) & -> Environment& {
    ambient_light = ambient;
    directional_light = directional;
    return *this;
  }

  auto with_lighting(AmbientLight ambient, DirectionalLight directional) && -> Environment&& {
    ambient_light = ambient;
    directional_light = directional;
    return std::move(*this);
  }

  static auto from_color(Color clear_color) -> Environment {
    Environment environment{};
    environment.clear_color = clear_color;
    return environment;
  }

  static auto from_cubemap(gl::CubeMap cubemap, Color clear_color = ClearColor{}.color)
    -> Environment {
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
