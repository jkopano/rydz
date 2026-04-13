#pragma once

#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <cstring>
#include <string>

namespace rydz_gl {

class Skybox {
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

  unsigned int cubemap_id = 0;
  unsigned int vao = 0;
  unsigned int vbo = 0;
  bool loaded = false;

  Skybox() = default;

  static Skybox from(Config cfg) {
    Skybox skybox{};
    const std::string paths[6] = {cfg.right,  cfg.left,  cfg.top,
                                  cfg.bottom, cfg.front, cfg.back};

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
      enable_vertex_array(vao);
      draw_vertex_array(0, 36);
      disable_vertex_array();
    });

    disable_texture_cubemap();
    rl::rlEnableDepthMask();
    rl::rlEnableBackfaceCulling();
  }

  void unload() {
    if (cubemap_id != 0) {
      unload_texture_id(cubemap_id);
    }
    if (vao != 0) {
      unload_vertex_array(vao);
    }
    if (vbo != 0) {
      unload_vertex_buffer(vbo);
    }
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

    vao = load_vertex_array();
    enable_vertex_array(vao);
    vbo = load_vertex_buffer(vertices, sizeof(vertices), false);
    set_vertex_attribute(0, 3, RL_FLOAT, false, 0, 0);
    enable_vertex_attribute(0);
    disable_vertex_array();
  }

  static unsigned int load_cubemap_from_paths(const std::string paths[6]) {
    Image images[6];
    for (int i = 0; i < 6; ++i) {
      images[i] = load_image(paths[i]);
      if (images[i].data == nullptr) {
        for (int j = 0; j < i; ++j) {
          unload_image(images[j]);
        }
        return 0;
      }
      if (images[i].format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        image_format(images[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      }
    }

    int width = images[0].width;
    int height = images[0].height;
    int pixel_size = 4;

    auto *data = static_cast<unsigned char *>(
        RL_MALLOC(width * height * 6 * pixel_size));

    for (int i = 0; i < 6; ++i) {
      std::memcpy(data + i * width * height * pixel_size, images[i].data,
                  width * height * pixel_size);
      unload_image(images[i]);
    }

    unsigned int id =
        load_texture_cubemap(data, width, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    RL_FREE(data);
    return id;
  }
};

} // namespace rydz_gl
