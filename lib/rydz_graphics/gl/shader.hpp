#pragma once

#include "hash.hpp"
#include "rydz_graphics/gl/core.hpp"
#include <array>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace gl {

struct ShaderSpec {
  std::string vertex_path;
  std::string fragment_path;

  bool operator==(const ShaderSpec &other) const = default;

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
  UniformType type = UniformType::Float;
  int count = 1;

  union {
    std::array<float, 16> float_data;
    std::array<int, 4> int_data;
  };

  Uniform() : float_data{} {}

  explicit Uniform(float v) : float_data{} {
    type = UniformType::Float;
    float_data[0] = v;
  }

  Uniform(float x, float y) : float_data{} {
    type = UniformType::Vec2;
    float_data[0] = x;
    float_data[1] = y;
  }

  Uniform(float x, float y, float z) : float_data{} {
    type = UniformType::Vec3;
    float_data[0] = x;
    float_data[1] = y;
    float_data[2] = z;
  }

  Uniform(float x, float y, float z, float w) : float_data{} {
    type = UniformType::Vec4;
    float_data[0] = x;
    float_data[1] = y;
    float_data[2] = z;
    float_data[3] = w;
  }

  explicit Uniform(int v) : float_data{} {
    type = UniformType::Int;
    int_data[0] = v;
  }

  Uniform(int x, int y) : float_data{} {
    type = UniformType::IVec2;
    int_data[0] = x;
    int_data[1] = y;
  }

  explicit Uniform(const std::array<float, 16> &mat)
      : type(UniformType::Mat4), float_data(mat) {}

  Uniform(const math::Vec3 &v) : Uniform(v.GetX(), v.GetY(), v.GetZ()) {}
  bool operator==(const Uniform &other) const {
    if (type != other.type || count != other.count)
      return false;

    switch (type) {
    case UniformType::Int:
    case UniformType::IVec2:
    case UniformType::IVec3:
    case UniformType::IVec4:
      return int_data == other.int_data;
    default:
      return float_data == other.float_data;
    }
  }
};

class ShaderProgram {
public:
  ShaderProgram() = default;

  explicit ShaderProgram(Shader shader, bool owns_resource = true)
      : shader_(shader), owns_resource_(owns_resource) {}

  ShaderProgram(const ShaderProgram &) = delete;
  ShaderProgram &operator=(const ShaderProgram &) = delete;

  ShaderProgram(ShaderProgram &&other) noexcept
      : shader_(other.shader_),
        uniform_locations_(std::move(other.uniform_locations_)),
        attribute_locations_(std::move(other.attribute_locations_)),
        owns_resource_(other.owns_resource_) {
    other.shader_ = Shader{};
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
    other.shader_ = Shader{};
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
    Shader shader{};
    shader.id = default_shader_id();
    shader.locs = default_shader_locs();
    return ShaderProgram(shader, false);
  }

  Shader &raw() { return shader_; }
  const Shader &raw() const { return shader_; }

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

  void set(const char *name, const Vec2 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC2);
  }

  void set(const char *name, const Vec3 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC3);
  }

  void set(const char *name, const Vec4 &value) {
    set_value(name, &value, SHADER_UNIFORM_VEC4);
  }

  void set(const char *name, const Matrix &value) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueMatrix(shader_, location, value);
    }
  }

  void set(const char *name, const math::Mat4 &value) {
    set(name, math::to_rl(value));
  }

  void set(const std::string &name, float value) { set(name.c_str(), value); }
  void set(const std::string &name, int value) { set(name.c_str(), value); }
  void set(const std::string &name, const Vec2 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const Vec3 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const Vec4 &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const Matrix &value) {
    set(name.c_str(), value);
  }
  void set(const std::string &name, const math::Mat4 &value) {
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

  void set_texture(const char *name, const Texture &texture) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  void set_texture(int location, const Texture &texture) {
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  void apply(const std::string &name, const Uniform &uniform) {
    switch (uniform.type) {
    case UniformType::Float:
      if (uniform.count > 1) {
        set_float_array(name.c_str(), uniform.float_data.data(), uniform.count);
      } else {
        set(name.c_str(), uniform.float_data[0]);
      }
      break;
    case UniformType::Vec2:
      set(name.c_str(), Vec2{uniform.float_data[0], uniform.float_data[1]});
      break;
    case UniformType::Vec3:
      set(name.c_str(), Vec3{uniform.float_data[0], uniform.float_data[1],
                             uniform.float_data[2]});
      break;
    case UniformType::Vec4:
      set(name.c_str(), Vec4{uniform.float_data[0], uniform.float_data[1],
                             uniform.float_data[2], uniform.float_data[3]});
      break;
    case UniformType::Int:
      set(name.c_str(), uniform.int_data[0]);
      break;
    case UniformType::IVec2:
      set_ints(name.c_str(), uniform.int_data.data(), SHADER_UNIFORM_IVEC2);
      break;
    case UniformType::IVec3:
      set_ints(name.c_str(), uniform.int_data.data(), SHADER_UNIFORM_IVEC3);
      break;
    case UniformType::IVec4:
      set_ints(name.c_str(), uniform.int_data.data(), SHADER_UNIFORM_IVEC4);
      break;
    case UniformType::Mat4: {
      const Matrix matrix = {
          uniform.float_data[0],  uniform.float_data[1],
          uniform.float_data[2],  uniform.float_data[3],
          uniform.float_data[4],  uniform.float_data[5],
          uniform.float_data[6],  uniform.float_data[7],
          uniform.float_data[8],  uniform.float_data[9],
          uniform.float_data[10], uniform.float_data[11],
          uniform.float_data[12], uniform.float_data[13],
          uniform.float_data[14], uniform.float_data[15],
      };
      set(name.c_str(), matrix);
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
  Shader shader_{};
  std::unordered_map<std::string, int> uniform_locations_;
  std::unordered_map<std::string, int> attribute_locations_;
  bool owns_resource_ = false;

  void unload() {
    if (owns_resource_ && shader_.id != 0 &&
        shader_.id != default_shader_id()) {
      ::UnloadShader(shader_);
    }
    shader_ = Shader{};
    owns_resource_ = false;
  }

  void set_value(const char *name, const void *value, int type) {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValue(shader_, location, value, type);
    }
  }
};

inline int shader_location(const Shader &shader, const char *name) {
  return rl::GetShaderLocation(shader, name);
}

inline int shader_location_attrib(const Shader &shader, const char *name) {
  return rl::GetShaderLocationAttrib(shader, name);
}

} // namespace gl

namespace std {
template <> struct hash<gl::ShaderSpec> {
  size_t operator()(const gl::ShaderSpec &k) const noexcept {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<std::string>{}(k.vertex_path));
    rydz::hash_combine(seed, std::hash<std::string>{}(k.fragment_path));
    return seed;
  }
};

template <> struct hash<gl::Uniform> {
  size_t operator()(const gl::Uniform &k) const noexcept {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.type)));
    rydz::hash_combine(seed, std::hash<int>{}(k.count));
    for (float value : k.float_data) {
      rydz::hash_combine(seed, std::hash<float>{}(value));
    }
    for (int value : k.int_data) {
      rydz::hash_combine(seed, std::hash<int>{}(value));
    }
    return seed;
  }
};
} // namespace std
