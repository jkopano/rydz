#pragma once

#include "hash.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/shader_bindings.hpp"
#include <array>
#include <concepts>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace gl {

struct ShaderSpec {
  std::string vertex_path;
  std::string fragment_path;

  auto operator==(const ShaderSpec &other) const -> bool = default;

  static auto from(std::string vertex_path, std::string fragment_path)
      -> ShaderSpec {
    return {
        .vertex_path = std::move(vertex_path),
        .fragment_path = std::move(fragment_path),
    };
  }

  auto empty() const -> bool {
    return vertex_path.empty() || fragment_path.empty();
  }
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

  explicit Uniform(int v) : int_data{} {
    type = UniformType::Int;
    int_data[0] = v;
  }

  Uniform(int x, int y) : int_data{} {
    type = UniformType::IVec2;
    int_data[0] = x;
    int_data[1] = y;
  }

  Uniform(int x, int y, int z) : int_data{} {
    type = UniformType::IVec3;
    int_data[0] = x;
    int_data[1] = y;
    int_data[2] = z;
  }

  Uniform(int x, int y, int z, int w) : int_data{} {
    type = UniformType::IVec4;
    int_data[0] = x;
    int_data[1] = y;
    int_data[2] = z;
    int_data[3] = w;
  }

  explicit Uniform(const std::array<float, 16> &mat)
      : type(UniformType::Mat4), float_data(mat) {}

  Uniform(const math::Vec3 &v) : Uniform(v.x, v.y, v.z) {}
  auto operator==(const Uniform &other) const -> bool {
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
  auto operator=(const ShaderProgram &) -> ShaderProgram & = delete;

  ShaderProgram(ShaderProgram &&other) noexcept
      : shader_(other.shader_),
        uniform_locations_(std::move(other.uniform_locations_)),
        attribute_locations_(std::move(other.attribute_locations_)),
        owns_resource_(other.owns_resource_) {
    other.shader_ = Shader{};
    other.owns_resource_ = false;
  }

  auto operator=(ShaderProgram &&other) noexcept -> ShaderProgram & {
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

  static auto load(const ShaderSpec &spec) -> ShaderProgram {
    if (spec.empty()) {
      return default_shader();
    }

    return ShaderProgram(
        rl::LoadShader(spec.vertex_path.c_str(), spec.fragment_path.c_str()));
  }

  static auto default_shader() -> ShaderProgram {
    Shader shader{};
    shader.id = Shader::default_id();
    shader.locs = Shader::default_locs();
    return ShaderProgram(shader, false);
  }

  auto raw() -> Shader & { return shader_; }
  auto raw() const -> const Shader & { return shader_; }

  auto uniform_location(const std::string_view name) -> int {
    std::string owned_name{name};
    auto iter = uniform_locations_.find(owned_name);
    if (iter != uniform_locations_.end()) {
      return iter->second;
    }

    const int location = rl::GetShaderLocation(shader_, owned_name.c_str());
    uniform_locations_.emplace(std::move(owned_name), location);
    return location;
  }

  auto attribute_location(const char *name) -> int {
    auto iter = attribute_locations_.find(name);
    if (iter != attribute_locations_.end()) {
      return iter->second;
    }

    const int location = rl::GetShaderLocationAttrib(shader_, name);
    attribute_locations_.emplace(name, location);
    return location;
  }

  auto set(const std::string_view name, float value) -> void {
    set_value(name, &value, SHADER_UNIFORM_FLOAT);
  }

  auto set(const std::string_view name, int value) -> void {
    set_value(name, &value, SHADER_UNIFORM_INT);
  }

  auto set(const std::string_view name, const Vec2 &value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC2);
  }

  auto set(const std::string_view name, const Vec3 &value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC3);
  }

  auto set(const std::string_view name, const Vec4 &value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC4);
  }

  auto set(const std::string_view name, const std::array<int, 2> &vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC2);
  }

  auto set(const std::string_view name, const std::array<int, 3> &vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC3);
  }

  auto set(const std::string_view name, const std::array<int, 4> &vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC4);
  }

  auto set(const std::string_view name, const Matrix &value) -> void {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueMatrix(shader_, location, value);
    }
  }

  auto set(const std::string_view name, const math::Mat4 &value) -> void {
    set(name, math::to_rl(value));
  }

  auto set(std::string_view name, std::span<const float> values) -> void {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueV(shader_, location, values.data(),
                          SHADER_UNIFORM_FLOAT,
                          static_cast<int>(values.size()));
    }
  }

  auto set(const std::string_view name, const Texture &texture) -> void {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  template <ecs::ShaderUniformBinding Binding, typename T>
  auto set(Binding binding, T &&value) -> void {
    set(map_uniform_binding(binding), std::forward<T>(value));
  }

  template <ecs::ShaderTextureBinding Binding>
  auto set_texture(Binding binding, const Texture &texture) -> void {
    set(map_texture_binding(binding), texture);
  }

  auto set(int location, const Texture &texture) -> void {
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  auto apply(const std::string &name, const Uniform &uniform) -> void {
    switch (uniform.type) {
    case UniformType::Float:
      if (uniform.count > 1) {
        size_t count = static_cast<size_t>(uniform.count);
        if (count > uniform.float_data.size()) {
          count = uniform.float_data.size();
        }
        set(name, std::span<const float>{uniform.float_data.data(), count});
      } else {
        set(name, uniform.float_data[0]);
      }
      break;
    case UniformType::Vec2:
      set(name, Vec2{uniform.float_data[0], uniform.float_data[1]});
      break;
    case UniformType::Vec3:
      set(name, Vec3{uniform.float_data[0], uniform.float_data[1],
                     uniform.float_data[2]});
      break;
    case UniformType::Vec4:
      set(name, Vec4{uniform.float_data[0], uniform.float_data[1],
                     uniform.float_data[2], uniform.float_data[3]});
      break;
    case UniformType::Int:
      set(name, uniform.int_data[0]);
      break;
    case UniformType::IVec2:
      set(name, std::array<int, 2>{uniform.int_data[0], uniform.int_data[1]});
      break;
    case UniformType::IVec3:
      set(name, std::array<int, 3>{uniform.int_data[0], uniform.int_data[1],
                                   uniform.int_data[2]});
      break;
    case UniformType::IVec4:
      set(name, std::array<int, 4>{uniform.int_data[0], uniform.int_data[1],
                                   uniform.int_data[2], uniform.int_data[3]});
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
      set(name, matrix);
      break;
    }
    }
  }

  template <ecs::ShaderUniformBinding Binding>
  auto apply(Binding binding, const Uniform &uniform) -> void {
    apply(std::string{map_uniform_binding(binding)}, uniform);
  }

  template <typename F> auto with_bound(F &&fn) -> decltype(auto) {
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

  template <typename F> auto with_enabled(F &&fn) -> decltype(auto) {
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

private:
  Shader shader_{};
  std::unordered_map<std::string, int> uniform_locations_;
  std::unordered_map<std::string, int> attribute_locations_;
  bool owns_resource_ = false;

  auto unload() -> void {
    if (owns_resource_ && shader_.id != 0 &&
        shader_.id != Shader::default_id()) {
      ::UnloadShader(shader_);
    }
    shader_ = Shader{};
    owns_resource_ = false;
  }

  auto set_value(const std::string_view name, const void *value, int type)
      -> void {
    const int location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValue(shader_, location, value, type);
    }
  }
};

inline auto shader_location(const Shader &shader, const char *name) -> int {
  return rl::GetShaderLocation(shader, name);
}

inline auto shader_location_attrib(const Shader &shader, const char *name)
    -> int {
  return rl::GetShaderLocationAttrib(shader, name);
}

} // namespace gl

namespace std {
template <> struct hash<gl::ShaderSpec> {
  auto operator()(const gl::ShaderSpec &k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<std::string>{}(k.vertex_path));
    rydz::hash_combine(seed, std::hash<std::string>{}(k.fragment_path));
    return seed;
  }
};

template <> struct hash<gl::Uniform> {
  auto operator()(const gl::Uniform &k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.type)));
    rydz::hash_combine(seed, std::hash<int>{}(k.count));

    switch (k.type) {
    case gl::UniformType::Int:
    case gl::UniformType::IVec2:
    case gl::UniformType::IVec3:
    case gl::UniformType::IVec4:
      for (int value : k.int_data) {
        rydz::hash_combine(seed, std::hash<int>{}(value));
      }
      break;
    default:
      for (float value : k.float_data) {
        rydz::hash_combine(seed, std::hash<float>{}(value));
      }
      break;
    }
    return seed;
  }
};
} // namespace std
