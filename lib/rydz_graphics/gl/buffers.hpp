#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <concepts>
#include <external/glad.h>
#include <span>
#include <utility>

using namespace math;

namespace gl {

template <typename T> class BufferBuilder;

class SSBO final {
public:
  SSBO() = default;
  SSBO(unsigned int size, void const* data, int usage)
      : id_(rl::rlLoadShaderBuffer(size, data, usage)), size_(size) {}

  SSBO(const SSBO&) = delete;
  auto operator=(const SSBO&) -> SSBO& = delete;
  SSBO(SSBO&& other) noexcept
      : id_(std::exchange(other.id_, 0)), size_(std::exchange(other.size_, 0)) {}
  auto operator=(SSBO&& other) noexcept -> SSBO& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    size_ = std::exchange(other.size_, 0);
    return *this;
  }

  ~SSBO() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }
  [[nodiscard]] auto size() const -> usize { return size_; }

  template <typename T> static auto builder() -> BufferBuilder<T>;

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadShaderBuffer(id_);
      id_ = 0;
      size_ = 0;
    }
  }

  auto update(void const* data, unsigned int size, unsigned int offset = 0) const
    -> void {
    if (id_ != 0) {
      rl::rlUpdateShaderBuffer(id_, data, size, offset);
    }
  }

  template <typename T>
  auto update(std::span<T const> data, usize offset = 0) const -> bool {
    static_assert(
      std::is_trivially_copyable_v<T>, "Buffer element type must be trivially copyable"
    );

    usize const byte_offset = offset * sizeof(T);
    usize const byte_size = data.size() * sizeof(T);

    if (byte_offset + byte_size > size_) {
      warn(
        "SSBO::update: bounds violation - attempted to write {} bytes at offset {}, "
        "buffer size is {}",
        byte_size,
        byte_offset,
        size_
      );
      return false;
    }

    update(data.data(), static_cast<u32>(byte_size), static_cast<u32>(byte_offset));
    return true;
  }

  auto bind(unsigned int index) const -> void {
    if (id_ != 0) {
      rl::rlBindShaderBuffer(id_, index);
    }
  }

  auto unbind() const -> void {}

private:
  u32 id_ = 0;
  usize size_ = 0;
};

class UBO final {
public:
  UBO() = default;
  UBO(unsigned int size, void const* data, int usage) : size_(size) {
    rl::GenBuffers(1, &id_);
    rl::BindBuffer(GL_UNIFORM_BUFFER, id_);
    rl::BufferData(GL_UNIFORM_BUFFER, size, data, usage);
    rl::BindBuffer(GL_UNIFORM_BUFFER, 0);
  }

  UBO(const UBO&) = delete;
  UBO(UBO&& other) noexcept
      : id_(std::exchange(other.id_, 0)), size_(std::exchange(other.size_, 0)) {}
  auto operator=(const UBO&) -> UBO& = delete;
  auto operator=(UBO&& other) noexcept -> UBO& {
    if (this == &other) {
      return *this;
    }
    reset();
    id_ = std::exchange(other.id_, 0);
    size_ = std::exchange(other.size_, 0);
    return *this;
  }

  ~UBO() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }
  [[nodiscard]] auto size() const -> usize { return size_; }

  template <typename T> static auto builder() -> BufferBuilder<T>;

  auto reset() -> void {
    if (id_ != 0) {
      rl::DeleteBuffers(1, &id_);
      id_ = 0;
      size_ = 0;
    }
  }

  auto update(void const* data, unsigned int size, unsigned int offset = 0) const
    -> void {
    if (id_ != 0) {
      rl::BindBuffer(GL_UNIFORM_BUFFER, id_);
      rl::BufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
      rl::BindBuffer(GL_UNIFORM_BUFFER, 0);
    }
  }

  template <typename T>
  auto update(std::span<T const> data, usize offset = 0) const -> bool {
    static_assert(
      std::is_trivially_copyable_v<T>, "Buffer element type must be trivially copyable"
    );

    usize const byte_offset = offset * sizeof(T);
    usize const byte_size = data.size() * sizeof(T);

    if (byte_offset + byte_size > size_) {
      warn(
        "UBO::update: bounds violation - attempted to write {} bytes at offset {}, "
        "buffer size is {}",
        byte_size,
        byte_offset,
        size_
      );
      return false;
    }

    update(data.data(), static_cast<u32>(byte_size), static_cast<u32>(byte_offset));
    return true;
  }

  auto bind(unsigned int index) const -> void {
    if (id_ != 0) {
      rl::BindBufferBase(GL_UNIFORM_BUFFER, index, id_);
    }
  }

  auto unbind() const -> void { rl::BindBuffer(GL_UNIFORM_BUFFER, 0); }

private:
  u32 id_ = 0;
  usize size_ = 0;
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

#include "rydz_graphics/gl/buffer_builder.hpp"

namespace gl {

template <typename T> inline auto SSBO::builder() -> BufferBuilder<T> {
  return BufferBuilder<T>{};
}

template <typename T> inline auto UBO::builder() -> BufferBuilder<T> {
  return BufferBuilder<T>{};
}

} // namespace gl
