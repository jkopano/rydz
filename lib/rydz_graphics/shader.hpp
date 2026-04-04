#pragma once

#include "rl.hpp"
#include "types.hpp"
#include <array>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace ecs {

struct ShaderSpec {
  std::string vertex_path;
  std::string fragment_path;

  bool operator==(const ShaderSpec &o) const = default;

  static ShaderSpec from(std::string vertex_path, std::string fragment_path) {
    return {
        .vertex_path = std::move(vertex_path),
        .fragment_path = std::move(fragment_path),
    };
  }

  bool empty() const { return vertex_path.empty() || fragment_path.empty(); }
};

enum class UniformType {
  Float,
  Vec2,
  Vec3,
  Vec4,
  Int,
  IVec2,
  IVec3,
  IVec4,
  Mat4,
};

struct Uniform {
  std::string name;
  UniformType type = UniformType::Float;
  std::array<float, 16> float_data{};
  std::array<int, 4> int_data{};
  int count = 1;

  bool operator==(const Uniform &o) const = default;

  static Uniform float1(std::string name, f32 value) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Float;
    uniform.float_data[0] = value;
    return uniform;
  }

  static Uniform vec2(std::string name, f32 x, f32 y) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Vec2;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    return uniform;
  }

  static Uniform vec3(std::string name, f32 x, f32 y, f32 z) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Vec3;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    uniform.float_data[2] = z;
    return uniform;
  }

  static Uniform vec4(std::string name, f32 x, f32 y, f32 z, f32 w) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Vec4;
    uniform.float_data[0] = x;
    uniform.float_data[1] = y;
    uniform.float_data[2] = z;
    uniform.float_data[3] = w;
    return uniform;
  }

  static Uniform int1(std::string name, int value) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Int;
    uniform.int_data[0] = value;
    return uniform;
  }

  static Uniform ivec2(std::string name, int x, int y) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::IVec2;
    uniform.int_data[0] = x;
    uniform.int_data[1] = y;
    return uniform;
  }

  static Uniform ivec3(std::string name, int x, int y, int z) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::IVec3;
    uniform.int_data[0] = x;
    uniform.int_data[1] = y;
    uniform.int_data[2] = z;
    return uniform;
  }

  static Uniform ivec4(std::string name, int x, int y, int z, int w) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::IVec4;
    uniform.int_data[0] = x;
    uniform.int_data[1] = y;
    uniform.int_data[2] = z;
    uniform.int_data[3] = w;
    return uniform;
  }

  static Uniform mat4(std::string name, const std::array<float, 16> &value) {
    Uniform uniform;
    uniform.name = std::move(name);
    uniform.type = UniformType::Mat4;
    uniform.float_data = value;
    return uniform;
  }
};

class ShaderProgram {
public:
  ShaderProgram() = default;

  explicit ShaderProgram(rl::Shader shader, bool owns_resource = true)
      : shader_(shader), owns_resource_(owns_resource) {}

  ShaderProgram(const ShaderProgram &) = delete;
  ShaderProgram &operator=(const ShaderProgram &) = delete;

  ShaderProgram(ShaderProgram &&other) noexcept
      : shader_(other.shader_),
        uniform_locations_(std::move(other.uniform_locations_)),
        attribute_locations_(std::move(other.attribute_locations_)),
        owns_resource_(other.owns_resource_) {
    other.shader_ = {};
    other.owns_resource_ = false;
  }

  ShaderProgram &operator=(ShaderProgram &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    unload();
    shader_ = other.shader_;
    uniform_locations_ = std::move(other.uniform_locations_);
    attribute_locations_ = std::move(other.attribute_locations_);
    owns_resource_ = other.owns_resource_;
    other.shader_ = {};
    other.owns_resource_ = false;
    return *this;
  }

  ~ShaderProgram() { unload(); }

  static ShaderProgram load(const ShaderSpec &spec) {
    if (spec.empty()) {
      return default_shader();
    }

    return ShaderProgram(
        rl::LoadShader(spec.vertex_path.c_str(), spec.fragment_path.c_str()));
  }

  static ShaderProgram default_shader() {
    rl::Shader shader{};
    shader.id = rl::rlGetShaderIdDefault();
    shader.locs = rl::rlGetShaderLocsDefault();
    return ShaderProgram(shader, false);
  }

  rl::Shader &raw() { return shader_; }
  const rl::Shader &raw() const { return shader_; }

  int uniform_location(const char *name) {
    auto it = uniform_locations_.find(name);
    if (it != uniform_locations_.end()) {
      return it->second;
    }

    const int location = rl::GetShaderLocation(shader_, name);
    uniform_locations_.emplace(name, location);
    return location;
  }

  int attribute_location(const char *name) {
    auto it = attribute_locations_.find(name);
    if (it != attribute_locations_.end()) {
      return it->second;
    }

    const int location = rl::GetShaderLocationAttrib(shader_, name);
    attribute_locations_.emplace(name, location);
    return location;
  }

  void set(const char *name, float value) {
    set_value(name, &value, SHADER_UNIFORM_FLOAT);
  }

  void set(const char *name, int value) {
    set_value(name, &value, SHADER_UNIFORM_INT);
  }

  void set(const char *name, const rl::Vector2 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC2);
  }

  void set(const char *name, const rl::Vector3 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC3);
  }

  void set(const char *name, const rl::Vector4 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC4);
  }

  void set(const char *name, const rl::Matrix &value) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueMatrix(shader_, location, value);
    }
  }

  void set(const std::string &name, float value) { set(name.c_str(), value); }
  void set(const std::string &name, int value) { set(name.c_str(), value); }
  void set(const std::string &name, const rl::Vector2 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const rl::Vector3 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const rl::Vector4 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const rl::Matrix &value) {
    set(name.c_str(), value);
  }

  void set_float_array(const char *name, const float *value, int count) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueV(shader_, location, value, SHADER_UNIFORM_FLOAT,
                          count);
    }
  }

  void set_ints(const char *name, const int *value, int type) {
    set_value(name, value, type);
  }

  void set_texture(const char *name, const rl::Texture2D &texture) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  void set_texture(int location, const rl::Texture2D &texture) {
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  void apply(const Uniform &uniform) {
    switch (uniform.type) {
    case UniformType::Float:
      if (uniform.count > 1) {
        set_float_array(uniform.name.c_str(), uniform.float_data.data(),
                        uniform.count);
      } else {
        set(uniform.name.c_str(), uniform.float_data[0]);
      }
      break;
    case UniformType::Vec2:
      set(uniform.name.c_str(),
          rl::Vector2{uniform.float_data[0], uniform.float_data[1]});
      break;
    case UniformType::Vec3:
      set(uniform.name.c_str(),
          rl::Vector3{uniform.float_data[0], uniform.float_data[1],
                      uniform.float_data[2]});
      break;
    case UniformType::Vec4:
      set(uniform.name.c_str(),
          rl::Vector4{uniform.float_data[0], uniform.float_data[1],
                      uniform.float_data[2], uniform.float_data[3]});
      break;
    case UniformType::Int:
      set(uniform.name.c_str(), uniform.int_data[0]);
      break;
    case UniformType::IVec2:
      set_ints(uniform.name.c_str(), uniform.int_data.data(),
               SHADER_UNIFORM_IVEC2);
      break;
    case UniformType::IVec3:
      set_ints(uniform.name.c_str(), uniform.int_data.data(),
               SHADER_UNIFORM_IVEC3);
      break;
    case UniformType::IVec4:
      set_ints(uniform.name.c_str(), uniform.int_data.data(),
               SHADER_UNIFORM_IVEC4);
      break;
    case UniformType::Mat4: {
      const rl::Matrix matrix = {
          uniform.float_data[0],  uniform.float_data[1],
          uniform.float_data[2],  uniform.float_data[3],
          uniform.float_data[4],  uniform.float_data[5],
          uniform.float_data[6],  uniform.float_data[7],
          uniform.float_data[8],  uniform.float_data[9],
          uniform.float_data[10], uniform.float_data[11],
          uniform.float_data[12], uniform.float_data[13],
          uniform.float_data[14], uniform.float_data[15],
      };
      set(uniform.name.c_str(), matrix);
      break;
    }
    }
  }

  template <typename F> decltype(auto) with_bound(F &&fn) {
    rl::BeginShaderMode(shader_);
    try {
      if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        std::invoke(std::forward<F>(fn));
        rl::EndShaderMode();
      } else {
        decltype(auto) result = std::invoke(std::forward<F>(fn));
        rl::EndShaderMode();
        return result;
      }
    } catch (...) {
      rl::EndShaderMode();
      throw;
    }
  }

  template <typename F> decltype(auto) with_enabled(F &&fn) {
    rl::rlEnableShader(shader_.id);
    try {
      if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        std::invoke(std::forward<F>(fn));
        rl::rlDisableShader();
      } else {
        decltype(auto) result = std::invoke(std::forward<F>(fn));
        rl::rlDisableShader();
        return result;
      }
    } catch (...) {
      rl::rlDisableShader();
      throw;
    }
  }

private:
  rl::Shader shader_{};
  std::unordered_map<std::string, int> uniform_locations_;
  std::unordered_map<std::string, int> attribute_locations_;
  bool owns_resource_ = false;

  void unload() {
    if (owns_resource_ && shader_.id != 0 &&
        shader_.id != rl::rlGetShaderIdDefault()) {
      ::UnloadShader(shader_);
    }
    shader_ = {};
    owns_resource_ = false;
  }

  void set_value(const char *name, const void *value, int type) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValue(shader_, location, value, type);
    }
  }
};

} // namespace ecs
