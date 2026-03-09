#pragma once
#include <cstring>
#include <raylib.h>
#include <rcamera.h>
#include <rlgl.h>
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
  Shader shader = {};
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

  void draw(Camera3D cam) const {
    if (!loaded)
      return;

    rlDisableBackfaceCulling();
    rlDisableDepthMask();

    rlEnableShader(shader.id);

    Matrix view_mat = GetCameraViewMatrix(&cam);
    float aspect = static_cast<float>(GetScreenWidth()) /
                   static_cast<float>(GetScreenHeight());
    Matrix proj_mat = GetCameraProjectionMatrix(&cam, aspect);

    int view_loc = GetShaderLocation(shader, "u_view");
    int proj_loc = GetShaderLocation(shader, "u_projection");

    SetShaderValueMatrix(shader, view_loc, view_mat);
    SetShaderValueMatrix(shader, proj_loc, proj_mat);

    rlActiveTextureSlot(0);
    rlEnableTextureCubemap(cubemap_id);

    rlEnableVertexArray(vao);
    rlDrawVertexArray(0, 36);
    rlDisableVertexArray();

    rlDisableTextureCubemap();
    rlDisableShader();

    rlEnableDepthMask();
    rlEnableBackfaceCulling();
  }

  void unload() {
    if (cubemap_id != 0)
      rlUnloadTexture(cubemap_id);
    if (vao != 0)
      rlUnloadVertexArray(vao);
    if (vbo != 0)
      rlUnloadVertexBuffer(vbo);
    loaded = false;
  }

private:
  static Shader get_skybox_shader() {
    static Shader s = {};
    static bool init = false;
    if (!init) {
      s = LoadShader("res/shaders/skybox.vert", "res/shaders/skybox.frag");
      int skybox_loc = GetShaderLocation(s, "u_skybox");
      int val = 0;
      SetShaderValue(s, skybox_loc, &val, SHADER_UNIFORM_INT);
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

    vao = rlLoadVertexArray();
    rlEnableVertexArray(vao);
    vbo = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
  }

  static unsigned int load_cubemap_from_paths(const std::string paths[6]) {
    Image images[6];
    for (int i = 0; i < 6; i++) {
      images[i] = LoadImage(paths[i].c_str());
      if (images[i].data == nullptr) {
        for (int j = 0; j < i; j++)
          UnloadImage(images[j]);
        return 0;
      }
      if (images[i].format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&images[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
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
      UnloadImage(images[i]);
    }

    unsigned int id =
        rlLoadTextureCubemap(data, w, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    RL_FREE(data);
    return id;
  }
};

} // namespace ecs
