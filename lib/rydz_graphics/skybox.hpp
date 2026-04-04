#pragma once
#include "math.hpp"
#include "rl.hpp"
#include "shader.hpp"
#include <cstring>
#include <string>

namespace ecs {

class Skybox {
public:
  struct Config {
    std::string right = "";
    std::string left = "";
    std::string top = "";
    std::string bottom = "";
    std::string front = "";
    std::string back = "";

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

    shader.set("u_view", math::to_rl(view));
    shader.set("u_projection", math::to_rl(proj));

    rl::rlActiveTextureSlot(0);
    rl::rlEnableTextureCubemap(cubemap_id);

    shader.with_enabled([&] {
      rl::rlEnableVertexArray(vao);
      rl::rlDrawVertexArray(0, 36);
      rl::rlDisableVertexArray();
    });

    rl::rlDisableTextureCubemap();
    rl::rlEnableDepthMask();
    rl::rlEnableBackfaceCulling();
  }

  void unload() {
    if (cubemap_id != 0)
      rl::rlUnloadTexture(cubemap_id);
    if (vao != 0)
      rl::rlUnloadVertexArray(vao);
    if (vbo != 0)
      rl::rlUnloadVertexBuffer(vbo);
    loaded = false;
  }

private:
  static ShaderProgram &get_skybox_shader() {
    static ShaderProgram shader = [] {
      ShaderProgram program = ShaderProgram::load(
          ShaderSpec::from_files("res/shaders/skybox.vert",
                                 "res/shaders/skybox.frag"));
      int val = 0;
      program.set("u_skybox", val);
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

    vao = rl::rlLoadVertexArray();
    rl::rlEnableVertexArray(vao);
    vbo = rl::rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rl::rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rl::rlEnableVertexAttribute(0);
    rl::rlDisableVertexArray();
  }

  static unsigned int load_cubemap_from_paths(const std::string paths[6]) {
    rl::Image images[6];
    for (int i = 0; i < 6; i++) {
      images[i] = rl::LoadImage(paths[i].c_str());
      if (images[i].data == nullptr) {
        for (int j = 0; j < i; j++)
          rl::UnloadImage(images[j]);
        return 0;
      }
      if (images[i].format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        rl::ImageFormat(&images[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      }
    }

    int w = images[0].width;
    int h = images[0].height;
    int pixel_size = 4;

    auto *data =
        static_cast<unsigned char *>(RL_MALLOC(w * h * 6 * pixel_size));

    for (int i = 0; i < 6; i++) {
      std::memcpy(data + i * w * h * pixel_size, images[i].data,
                  w * h * pixel_size);
      rl::UnloadImage(images[i]);
    }

    unsigned int id =
        rl::rlLoadTextureCubemap(data, w, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    RL_FREE(data);
    return id;
  }
};

} // namespace ecs
