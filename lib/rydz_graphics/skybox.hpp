#pragma once
#include "math.hpp"
#include "rl.hpp"
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
  rl::Shader shader = {};
  unsigned int vao = 0;
  unsigned int vbo = 0;
  bool loaded = false;

  Skybox() = default;

  static Skybox from(Config cfg) {
    Skybox skybox{};
    skybox.shader = get_skybox_shader();

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
    if (!loaded)
      return;

    rl::rlDisableBackfaceCulling();
    rl::rlDisableDepthMask();

    rl::rlEnableShader(shader.id);

    int view_loc = rl::GetShaderLocation(shader, "u_view");
    int proj_loc = rl::GetShaderLocation(shader, "u_projection");

    rl::SetShaderValueMatrix(shader, view_loc, math::to_rl(view));
    rl::SetShaderValueMatrix(shader, proj_loc, math::to_rl(proj));

    rl::rlActiveTextureSlot(0);
    rl::rlEnableTextureCubemap(cubemap_id);

    rl::rlEnableVertexArray(vao);
    rl::rlDrawVertexArray(0, 36);
    rl::rlDisableVertexArray();

    rl::rlDisableTextureCubemap();
    rl::rlDisableShader();

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
  static rl::Shader get_skybox_shader() {
    static rl::Shader s = {};
    static bool init = false;
    if (!init) {
      s = rl::LoadShader("res/shaders/skybox.vert", "res/shaders/skybox.frag");
      int skybox_loc = rl::GetShaderLocation(s, "u_skybox");
      int val = 0;
      rl::SetShaderValue(s, skybox_loc, &val, SHADER_UNIFORM_INT);
      init = true;
    }
    return s;
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
