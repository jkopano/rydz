#pragma once

#include "hash.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_graphics/gl/shader_bindings.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <algorithm>
#include <array>
#include <concepts>
#include <external/glad.h>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

using namespace math;

namespace gl {

struct ShaderSpec {
  std::string vertex_path;
  std::string fragment_path;

  auto operator==(ShaderSpec const& other) const -> bool = default;

  static auto from(std::string vertex_path, std::string fragment_path) -> ShaderSpec {
    return {
      .vertex_path = std::move(vertex_path),
      .fragment_path = std::move(fragment_path),
    };
  }

  [[nodiscard]] auto empty() const -> bool {
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
  int count{1};

  union {
    std::array<float, 16> float_data;
    std::array<int, 4> int_data;
  };

  Uniform() : float_data{} {}

  explicit Uniform(float v) : float_data{} { float_data[0] = v; }

  Uniform(float x, float y) : type(UniformType::Vec2), float_data{} {
    float_data[0] = x;
    float_data[1] = y;
  }

  Uniform(float x, float y, float z) : type(UniformType::Vec3), float_data{} {
    float_data[0] = x;
    float_data[1] = y;
    float_data[2] = z;
  }

  Uniform(float x, float y, float z, float w) : type(UniformType::Vec4), float_data{} {
    float_data[0] = x;
    float_data[1] = y;
    float_data[2] = z;
    float_data[3] = w;
  }

  explicit Uniform(int v) : type(UniformType::Int), int_data{} { int_data[0] = v; }

  Uniform(int x, int y) : type(UniformType::IVec2), int_data{} {
    int_data[0] = x;
    int_data[1] = y;
  }

  Uniform(int x, int y, int z) : type(UniformType::IVec3), int_data{} {
    int_data[0] = x;
    int_data[1] = y;
    int_data[2] = z;
  }

  Uniform(int x, int y, int z, int w) : type(UniformType::IVec4), int_data{} {
    int_data[0] = x;
    int_data[1] = y;
    int_data[2] = z;
    int_data[3] = w;
  }

  explicit Uniform(std::array<float, 16> const& mat)
      : type(UniformType::Mat4), float_data(mat) {}

  Uniform(math::Vec3 const& v) : Uniform(v.x, v.y, v.z) {}
  auto operator==(Uniform const& other) const -> bool {
    if (type != other.type || count != other.count) {
      return false;
    }

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

  ShaderProgram(ShaderProgram const&) = delete;
  auto operator=(ShaderProgram const&) -> ShaderProgram& = delete;

  ShaderProgram(ShaderProgram&& other) noexcept
      : shader_(other.shader_), uniform_locations_(std::move(other.uniform_locations_)),
        attribute_locations_(std::move(other.attribute_locations_)),
        uniform_block_indices_(std::move(other.uniform_block_indices_)),
        uniform_block_bindings_(std::move(other.uniform_block_bindings_)),
        owns_resource_(other.owns_resource_) {
    other.shader_ = Shader{};
    other.owns_resource_ = false;
  }

  auto operator=(ShaderProgram&& other) noexcept -> ShaderProgram& {
    if (this == &other) {
      return *this;
    }

    unload();
    shader_ = other.shader_;
    uniform_locations_ = std::move(other.uniform_locations_);
    attribute_locations_ = std::move(other.attribute_locations_);
    uniform_block_indices_ = std::move(other.uniform_block_indices_);
    uniform_block_bindings_ = std::move(other.uniform_block_bindings_);
    owns_resource_ = other.owns_resource_;
    other.shader_ = Shader{};
    other.owns_resource_ = false;
    return *this;
  }

  ~ShaderProgram() { unload(); }

  static auto load(ShaderSpec const& spec) -> ShaderProgram {
    if (spec.empty()) {
      return default_shader();
    }

    return ShaderProgram(
      rl::LoadShader(spec.vertex_path.c_str(), spec.fragment_path.c_str())
    );
  }

  static auto default_shader() -> ShaderProgram {
    Shader shader{};
    shader.id = Shader::default_id();
    shader.locs = Shader::default_locs();
    return ShaderProgram(shader, false);
  }

  auto raw() -> Shader& { return shader_; }
  auto raw() const -> Shader const& { return shader_; }

  auto uniform_location(std::string_view const name) -> int {
    std::string owned_name{name};
    auto iter = uniform_locations_.find(owned_name);
    if (iter != uniform_locations_.end()) {
      return iter->second;
    }

    int const location = rl::GetShaderLocation(shader_, owned_name.c_str());
    uniform_locations_.emplace(std::move(owned_name), location);
    return location;
  }

  auto attribute_location(char const* name) -> int {
    auto iter = attribute_locations_.find(name);
    if (iter != attribute_locations_.end()) {
      return iter->second;
    }

    int const location = rl::GetShaderLocationAttrib(shader_, name);
    attribute_locations_.emplace(name, location);
    return location;
  }

  auto uniform_block_index(std::string_view const name) -> unsigned int {
    std::string owned_name{name};
    auto iter = uniform_block_indices_.find(owned_name);
    if (iter != uniform_block_indices_.end()) {
      return iter->second;
    }

    unsigned int const index = glGetUniformBlockIndex(shader_.id, owned_name.c_str());
    uniform_block_indices_.emplace(std::move(owned_name), index);
    return index;
  }

  auto bind_uniform_block(std::string_view const name, unsigned int binding) -> bool {
    unsigned int const index = uniform_block_index(name);
    if (index == GL_INVALID_INDEX) {
      return false;
    }

    auto iter = uniform_block_bindings_.find(index);
    if (iter != uniform_block_bindings_.end() && iter->second == binding) {
      return true;
    }

    glUniformBlockBinding(shader_.id, index, binding);
    uniform_block_bindings_.insert_or_assign(index, binding);
    return true;
  }

  auto set(std::string_view const name, float value) -> void {
    set_value(name, &value, SHADER_UNIFORM_FLOAT);
  }

  auto set(std::string_view const name, int value) -> void {
    set_value(name, &value, SHADER_UNIFORM_INT);
  }

  auto set(std::string_view const name, Vec2 const& value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC2);
  }

  auto set(std::string_view const name, Vec3 const& value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC3);
  }

  auto set(std::string_view const name, Vec4 const& value) -> void {
    set_value(name, &value, SHADER_UNIFORM_VEC4);
  }

  auto set(std::string_view const name, std::array<int, 2> const& vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC2);
  }

  auto set(std::string_view const name, std::array<int, 3> const& vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC3);
  }

  auto set(std::string_view const name, std::array<int, 4> const& vec) -> void {
    set_value(name, vec.data(), SHADER_UNIFORM_IVEC4);
  }

  auto set(std::string_view const name, Matrix const& value) -> void {
    int const location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueMatrix(shader_, location, value);
    }
  }

  auto set(std::string_view const name, math::Mat4 const& value) -> void {
    set(name, math::to_rl(value));
  }

  auto set(std::string_view name, std::span<float const> values) -> void {
    int const location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueV(
        shader_,
        location,
        values.data(),
        SHADER_UNIFORM_FLOAT,
        static_cast<int>(values.size())
      );
    }
  }

  auto set(std::string_view const name, Texture const& texture) -> void {
    int const location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  template <ecs::ShaderUniformBinding Binding, typename T>
  auto set(Binding binding, T&& value) -> void {
    set(map_uniform_binding(binding), std::forward<T>(value));
  }

  template <ecs::ShaderTextureBinding Binding>
  auto set_texture(Binding binding, Texture const& texture) -> void {
    set(map_texture_binding(binding), texture);
  }

  auto set(int location, Texture const& texture) -> void {
    if (location >= 0) {
      rl::SetShaderValueTexture(shader_, location, texture);
    }
  }

  auto apply(std::string const& name, Uniform const& uniform) -> void {
    switch (uniform.type) {
    case UniformType::Float:
      if (uniform.count > 1) {
        auto count = static_cast<usize>(uniform.count);
        count = std::min(count, uniform.float_data.size());
        set(name, std::span<float const>{uniform.float_data.data(), count});
      } else {
        set(name, uniform.float_data[0]);
      }
      break;
    case UniformType::Vec2:
      set(name, Vec2{uniform.float_data[0], uniform.float_data[1]});
      break;
    case UniformType::Vec3:
      set(
        name, Vec3{uniform.float_data[0], uniform.float_data[1], uniform.float_data[2]}
      );
      break;
    case UniformType::Vec4:
      set(
        name,
        Vec4{
          uniform.float_data[0],
          uniform.float_data[1],
          uniform.float_data[2],
          uniform.float_data[3]
        }
      );
      break;
    case UniformType::Int:
      set(name, uniform.int_data[0]);
      break;
    case UniformType::IVec2:
      set(name, std::array<int, 2>{uniform.int_data[0], uniform.int_data[1]});
      break;
    case UniformType::IVec3:
      set(
        name,
        std::array<int, 3>{uniform.int_data[0], uniform.int_data[1], uniform.int_data[2]}
      );
      break;
    case UniformType::IVec4:
      set(
        name,
        std::array<int, 4>{
          uniform.int_data[0],
          uniform.int_data[1],
          uniform.int_data[2],
          uniform.int_data[3]
        }
      );
      break;
    case UniformType::Mat4: {
      Matrix const matrix = {
        uniform.float_data[0],
        uniform.float_data[1],
        uniform.float_data[2],
        uniform.float_data[3],
        uniform.float_data[4],
        uniform.float_data[5],
        uniform.float_data[6],
        uniform.float_data[7],
        uniform.float_data[8],
        uniform.float_data[9],
        uniform.float_data[10],
        uniform.float_data[11],
        uniform.float_data[12],
        uniform.float_data[13],
        uniform.float_data[14],
        uniform.float_data[15],
      };
      set(name, matrix);
      break;
    }
    }
  }

  template <ecs::ShaderUniformBinding Binding>
  auto apply(Binding binding, Uniform const& uniform) -> void {
    apply(std::string{map_uniform_binding(binding)}, uniform);
  }

  template <typename F> auto with_bound(F&& fn) -> decltype(auto) {
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

  template <typename F> auto with_enabled(F&& fn) -> decltype(auto) {
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
  Shader shader_;
  std::unordered_map<std::string, int> uniform_locations_;
  std::unordered_map<std::string, int> attribute_locations_;
  std::unordered_map<std::string, unsigned int> uniform_block_indices_;
  std::unordered_map<unsigned int, unsigned int> uniform_block_bindings_;
  bool owns_resource_ = false;

  auto unload() -> void {
    if (owns_resource_ && shader_.id != 0 && shader_.id != Shader::default_id()) {
      ::UnloadShader(shader_);
    }
    shader_ = Shader{};
    owns_resource_ = false;
  }

  auto set_value(std::string_view const name, void const* value, int type) -> void {
    int const location = uniform_location(name);
    if (location >= 0) {
      rl::SetShaderValue(shader_, location, value, type);
    }
  }
};

inline auto shader_location(Shader const& shader, char const* name) -> int {
  return rl::GetShaderLocation(shader, name);
}

inline auto shader_location_attrib(Shader const& shader, char const* name) -> int {
  return rl::GetShaderLocationAttrib(shader, name);
}

} // namespace gl

namespace std {
template <> struct hash<gl::ShaderSpec> {
  auto operator()(gl::ShaderSpec const& k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<std::string>{}(k.vertex_path));
    rydz::hash_combine(seed, std::hash<std::string>{}(k.fragment_path));
    return seed;
  }
};

template <> struct hash<gl::Uniform> {
  auto operator()(gl::Uniform const& k) const noexcept -> size_t {
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
