#pragma once

#include "rydz_graphics/gl/buffers.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <array>

namespace gl {

class Skybox {
public:
  Skybox() = default;

  static auto create(ShaderProgram shader) -> Skybox;

  Skybox(Skybox const&) = delete;
  auto operator=(Skybox const&) -> Skybox& = delete;
  Skybox(Skybox&&) noexcept = default;
  auto operator=(Skybox&&) noexcept -> Skybox& = default;
  ~Skybox() = default;

  [[nodiscard]] auto ready() const -> bool {
    return vao_.ready() && shader_.raw().ready();
  }

  auto draw(CubeMap const& cubemap, math::Mat4 view, math::Mat4 proj) const -> void;

  auto unload() -> void {
    vao_.reset();
    vbo_.reset();
    shader_ = ShaderProgram{};
  }

private:
  VAO vao_{};
  VBO vbo_{};
  mutable ShaderProgram shader_{};

  auto build_cube_geometry() -> void;
};

inline auto Skybox::create(ShaderProgram shader) -> Skybox {
  Skybox skybox{};
  skybox.shader_ = std::move(shader);
  skybox.build_cube_geometry();
  return skybox;
}

inline auto Skybox::build_cube_geometry() -> void {

  std::array<f32, 108> vertices = {
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
    1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f,
    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
  };

  vao_ = VAO::create();
  if (vao_.bind()) {
    vbo_ = VBO::create(vertices.data(), sizeof(vertices), false);
    set_vertex_attribute(0, 3, RL_FLOAT, false, 0, 0);
    enable_vertex_attribute(0);
    VAO::unbind();
  }
}

inline auto Skybox::draw(CubeMap const& cubemap, math::Mat4 view, math::Mat4 proj) const
  -> void {
  if (!ready()) {
    return;
  }

  rl::rlDisableBackfaceCulling();
  rl::rlDisableDepthMask();

  cubemap.bind(0);

  shader_.with_enabled([&] -> void {
    shader_.set("u_mat_view", view);
    shader_.set("u_mat_projection", proj);
    shader_.set("u_skybox", 0);
    if (vao_.bind()) {
      vao_.draw(0, 36);
      VAO::unbind();
    }
  });

  cubemap.unbind();

  rl::rlEnableDepthMask();
  rl::rlEnableBackfaceCulling();
}

} // namespace gl
