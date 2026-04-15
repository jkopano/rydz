#pragma once

#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <cstring>
#include <string>

namespace gl {

class Skybox {
  static constexpr u32 FACE_COUNT = 6;

public:
  struct Config {
    std::string right{};
    std::string left{};
    std::string top{};
    std::string bottom{};
    std::string front{};
    std::string back{};

    static Config from_directory(const std::string &dir,
                                 const std::string &ext = ".jpg") {
      return {.right = dir + "/right" + ext,
              .left = dir + "/left" + ext,
              .top = dir + "/top" + ext,
              .bottom = dir + "/bottom" + ext,
              .front = dir + "/front" + ext,
              .back = dir + "/back" + ext};
    }
  };

  u32 cubemap_id = 0;
  VAO vao{};
  VBO vbo{};
  bool loaded = false;

  Skybox() = default;

  static Skybox from(Config cfg) {
    Skybox skybox{};
    const std::array<std::string, FACE_COUNT> paths = {
        cfg.right, cfg.left, cfg.top, cfg.bottom, cfg.front, cfg.back};

    skybox.cubemap_id = load_cubemap_from_paths(paths);
    skybox.create_cube_vao();
    skybox.loaded = (skybox.cubemap_id != 0);

    return skybox;
  }

  static Skybox from(const std::string &directory_path) {
    return Skybox::from(Config::from_directory(directory_path));
  }

  void draw(math::Mat4 view, math::Mat4 proj) const {
    if (!loaded) {
      return;
    }

    ShaderProgram &shader = get_skybox_shader();

    rl::rlDisableBackfaceCulling();
    rl::rlDisableDepthMask();

    shader.set("matView", view);
    shader.set("matProjection", proj);

    active_texture_slot(0);
    enable_texture_cubemap(cubemap_id);

    shader.with_enabled([&] {
      vao.bind();
      vao.draw(0, 36);
      VAO::unbind();
    });

    disable_texture_cubemap();
    rl::rlEnableDepthMask();
    rl::rlEnableBackfaceCulling();
  }

  void unload() {
    if (cubemap_id != 0) {
      unload_texture_id(cubemap_id);
      cubemap_id = 0;
    }
    vao.reset();
    vbo.reset();
    loaded = false;
  }

private:
  static ShaderProgram &get_skybox_shader() {
    static ShaderProgram shader = [] {
      ShaderProgram program = ShaderProgram::load(ShaderSpec::from(
          "res/shaders/skybox.vert", "res/shaders/skybox.frag"));
      int value = 0;
      program.set("u_skybox", value);
      return program;
    }();
    return shader;
  }

  void create_cube_vao() {
    float vertices[] = {
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
        -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
        -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

    vao = VAO::create();
    vao.bind();
    vbo = VBO::create(vertices, sizeof(vertices), false);
    set_vertex_attribute(0, 3, RL_FLOAT, false, 0, 0);
    enable_vertex_attribute(0);
    VAO::unbind();
  }

  static unsigned int
  load_cubemap_from_paths(const std::array<std::string, FACE_COUNT> &paths) {
    std::array<Image, FACE_COUNT> images;

    for (usize i = 0; i < images.size(); ++i) {
      images.at(i) = load_image(paths.at(i));
      if (images.at(i).data == nullptr) {
        for (usize j = 0; j < i; ++j) {
          unload_image(images.at(j));
        }
        return 0;
      }
      if (images.at(i).format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        image_format(images.at(i), PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      }
    }

    u32 width = images[0].width;
    u32 height = images[0].height;
    u32 pixel_size = 4U;

    std::vector<u8> data(width * height * FACE_COUNT * pixel_size);

    for (usize i = 0; i < FACE_COUNT; ++i) {
      u32 size = width * height * pixel_size;
      usize offset = i * size;
      std::memcpy(data.data() + offset, images.at(i).data,
                  width * height * pixel_size);
      unload_image(images.at(i));
    }

    unsigned int id = load_texture_cubemap(
        data.data(), width, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);

    return id;
  }
};

} // namespace gl
namespace ecs {
using Skybox = gl::Skybox;
}
