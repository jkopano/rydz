#pragma once

#include "rl.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <concepts>
#include <external/glad.h>
#include <utility>

namespace gl {

class ComputeProgram {
public:
  ComputeProgram() = default;
  ComputeProgram(ComputeProgram const&) = delete;
  auto operator=(ComputeProgram const&) -> ComputeProgram& = delete;

  ComputeProgram(ComputeProgram&& o) noexcept : id_(std::exchange(o.id_, 0)) {}
  auto operator=(ComputeProgram&& o) noexcept -> ComputeProgram& {
    if (this == &o) {
      return *this;
    }
    reset();
    id_ = std::exchange(o.id_, 0);
    return *this;
  }

  ~ComputeProgram() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadShaderProgram(id_);
      id_ = 0;
    }
  }

  auto load(char const* path) -> bool {
    reset();

    char* source = rl::LoadFileText(path);
    if (source == nullptr) {
      warn("failed to load compute shader '{}'", path);
      return false;
    }

    u32 const shader_id = rl::rlCompileShader(source, RL_COMPUTE_SHADER);
    rl::UnloadFileText(source);
    if (shader_id == 0) {
      warn("failed to compile compute shader '{}'", path);
      return false;
    }

    id_ = rl::rlLoadComputeShaderProgram(shader_id);
    glDeleteShader(shader_id);

    if (id_ == 0) {
      warn("failed to link compute shader '{}'", path);
      return false;
    }

    return true;
  }

private:
  u32 id_ = 0;
};

} // namespace gl

namespace ecs {

template <typename C>
concept IComputeShader = requires {
  { C::source_path() } -> std::convertible_to<char const*>;
};

} // namespace ecs
