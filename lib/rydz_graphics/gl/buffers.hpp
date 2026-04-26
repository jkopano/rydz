#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "types.hpp"
#include <concepts>
#include <external/glad.h>
#include <utility>

using namespace math;

namespace gl {

class SSBO final {
public:
  SSBO() = default;
  SSBO(unsigned int size, void const* data, int usage)
      : id_(rl::rlLoadShaderBuffer(size, data, usage)) {}

  SSBO(const SSBO&) = delete;
  auto operator=(const SSBO&) -> SSBO& = delete;
  SSBO(SSBO&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(SSBO&& other) noexcept -> SSBO& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~SSBO() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadShaderBuffer(id_);
      id_ = 0;
    }
  }
  auto update(void const* data, unsigned int size, unsigned int offset = 0) const
    -> void {
    if (id_ != 0) {
      rl::rlUpdateShaderBuffer(id_, data, size, offset);
    }
  }

  auto bind(unsigned int index) const -> void {
    if (id_ != 0) {
      rl::rlBindShaderBuffer(id_, index);
    }
  }

private:
  u32 id_ = 0;
};

class UBO final {
public:
  UBO() = default;
  UBO(unsigned int size, void const* data, int usage) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_UNIFORM_BUFFER, id_);
    glBufferData(GL_UNIFORM_BUFFER, size, data, usage);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }

  UBO(const UBO&) = delete;
  UBO(UBO&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(const UBO&) -> UBO& = delete;
  auto operator=(UBO&& other) noexcept -> UBO& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~UBO() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      glDeleteBuffers(1, &id_);
      id_ = 0;
    }
  }

  auto update(void const* data, unsigned int size, unsigned int offset = 0) const
    -> void {
    if (id_ != 0) {
      glBindBuffer(GL_UNIFORM_BUFFER, id_);
      glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
  }

  auto bind(unsigned int index) const -> void {
    if (id_ != 0) {
      glBindBufferBase(GL_UNIFORM_BUFFER, index, id_);
    }
  }

private:
  u32 id_ = 0;
};

class VertexArray final {
public:
  VertexArray() = default;

  static auto create() -> VertexArray {
    VertexArray array;
    array.id_ = rl::rlLoadVertexArray();
    return array;
  }

  VertexArray(VertexArray const&) = delete;
  auto operator=(VertexArray const&) -> VertexArray& = delete;
  VertexArray(VertexArray&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(VertexArray&& other) noexcept -> VertexArray& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexArray() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexArray(id_);
      id_ = 0;
    }
  }

  [[nodiscard]] auto bind() const -> bool {
    if (id_ == 0) {
      return false;
    }
    return rl::rlEnableVertexArray(id_);
  }

  static auto unbind() -> void { rl::rlDisableVertexArray(); }

  auto draw(i32 offset, i32 count) const -> void {
    if (id_ != 0) {
      rl::rlDrawVertexArray(offset, count);
    }
  }

private:
  u32 id_ = 0;
};

class VertexBuffer final {
public:
  VertexBuffer() = default;

  VertexBuffer(void const* data, i32 size, bool dynamic = false)
      : id_(rl::rlLoadVertexBuffer(data, size, dynamic)) {}

  static auto create(void const* data, i32 size, bool dynamic = false) -> VertexBuffer {
    return {data, size, dynamic};
  }

  VertexBuffer(VertexBuffer const&) = delete;
  auto operator=(VertexBuffer const&) -> VertexBuffer& = delete;
  VertexBuffer(VertexBuffer&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(VertexBuffer&& other) noexcept -> VertexBuffer& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexBuffer() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  auto bind() const -> void {
    if (id_ != 0) {
      rl::rlEnableVertexBuffer(id_);
    }
  }

  static auto unbind() -> void { rl::rlDisableVertexBuffer(); }

private:
  u32 id_ = 0;
};

class ElementBuffer final {
public:
  ElementBuffer() = default;

  ElementBuffer(void const* data, i32 size, bool dynamic = false)
      : id_(rl::rlLoadVertexBufferElement(data, size, dynamic)) {}

  static auto create(void const* data, i32 size, bool dynamic = false) -> ElementBuffer {
    return {data, size, dynamic};
  }

  ElementBuffer(ElementBuffer const&) = delete;
  auto operator=(ElementBuffer const&) -> ElementBuffer& = delete;
  ElementBuffer(ElementBuffer&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(ElementBuffer&& other) noexcept -> ElementBuffer& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~ElementBuffer() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  auto bind() const -> void {
    if (id_ != 0) {
      rl::rlEnableVertexBufferElement(id_);
    }
  }

  static auto unbind() -> void { rl::rlDisableVertexBufferElement(); }

  static auto draw(i32 offset, i32 count, void const* buffer = nullptr) -> void {
    rl::rlDrawVertexArrayElements(offset, count, buffer);
  }

private:
  u32 id_ = 0;
};

using VAO = VertexArray;
using VBO = VertexBuffer;
using EBO = ElementBuffer;

template <typename T>
concept GpuBuffer = requires(T const& b, T& m, void const* data, u32 sz, u32 idx) {
  { b.ready() } -> std::convertible_to<bool>;
  { b.id() } -> std::convertible_to<u32>;
  { m.reset() } -> std::same_as<void>;
  { b.update(data, sz, 0u) } -> std::same_as<void>;
  { b.bind(idx) } -> std::same_as<void>;
};

static_assert(GpuBuffer<SSBO>);
static_assert(GpuBuffer<UBO>);

} // namespace gl
