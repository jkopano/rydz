#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <concepts>
#include <external/glad.h>
#include <span>
#include <type_traits>
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

namespace gl {

enum class BufferUsage : u8 { Static, Dynamic, Stream };

template <typename T> class BufferBuilder {
  static_assert(
    std::is_trivially_copyable_v<T>, "Buffer element type must be trivially copyable"
  );

public:
  static auto with_data(std::span<T const> data) -> BufferBuilder<T> {
    BufferBuilder<T> builder;
    builder.source_type_ = SourceType::Data;
    builder.data_ = data;

    if (data.empty()) {
      builder.is_valid_ = false;
      warn("BufferBuilder::with_data: empty data span provided");
    }

    return builder;
  }
  static auto with_capacity(usize count) -> BufferBuilder<T> {
    BufferBuilder<T> builder;
    builder.source_type_ = SourceType::Capacity;
    builder.capacity_ = count;

    if (count == 0) {
      builder.is_valid_ = false;
      warn("BufferBuilder::with_capacity: zero capacity provided");
    }

    return builder;
  }

  auto dynamic(bool is_dynamic) -> BufferBuilder& {
    usage_ = is_dynamic ? BufferUsage::Dynamic : BufferUsage::Static;
    return *this;
  }

  auto usage(BufferUsage usage_hint) -> BufferBuilder& {
    usage_ = usage_hint;
    return *this;
  }

  auto build_ssbo() -> SSBO {
    if (!is_valid_) {
      warn("BufferBuilder::build_ssbo: builder is invalid, returning empty SSBO");
      return {};
    }

    usize const size = calculate_size();
    u32 const gl_usage = get_gl_usage();
    void const* data_ptr = get_data_ptr();

    return {static_cast<u32>(size), data_ptr, static_cast<i32>(gl_usage)};
  }

  auto build_ubo() -> UBO {
    if (!is_valid_) {
      warn("BufferBuilder::build_ubo: builder is invalid, returning empty UBO");
      return {};
    }

    usize const size = calculate_size();
    u32 const gl_usage = get_gl_usage();
    void const* data_ptr = get_data_ptr();

    return {static_cast<u32>(size), data_ptr, static_cast<i32>(gl_usage)};
  }

  auto build_vbo() -> VertexBuffer {
    if (!is_valid_) {
      warn("BufferBuilder::build_vbo: builder is invalid, returning empty VertexBuffer");
      return VertexBuffer{};
    }

    usize const size = calculate_size();
    void const* data_ptr = get_data_ptr();
    bool const is_dynamic =
      (usage_ == BufferUsage::Dynamic || usage_ == BufferUsage::Stream);

    return {data_ptr, static_cast<i32>(size), is_dynamic};
  }

  auto build_ebo() -> ElementBuffer {
    if (!is_valid_) {
      warn("BufferBuilder::build_ebo: builder is invalid, returning empty ElementBuffer");
      return ElementBuffer{};
    }

    usize const size = calculate_size();
    void const* data_ptr = get_data_ptr();
    bool const is_dynamic =
      (usage_ == BufferUsage::Dynamic || usage_ == BufferUsage::Stream);

    return {data_ptr, static_cast<i32>(size), is_dynamic};
  }

private:
  enum class SourceType : u8 { None, Data, Capacity };

  SourceType source_type_ = SourceType::None;
  std::span<T const> data_;
  usize capacity_ = 0;
  BufferUsage usage_ = BufferUsage::Static;
  bool is_valid_ = true;

  BufferBuilder() = default;

  [[nodiscard]] auto calculate_size() const -> usize {
    switch (source_type_) {
    case SourceType::Data:
      return data_.size() * sizeof(T);
    case SourceType::Capacity:
      return capacity_ * sizeof(T);
    case SourceType::None:
    default:
      return 0;
    }
  }
  [[nodiscard]] auto get_gl_usage() const -> u32 {
    switch (usage_) {
    case BufferUsage::Static:
      return GL_STATIC_DRAW;
    case BufferUsage::Dynamic:
      return GL_DYNAMIC_DRAW;
    case BufferUsage::Stream:
      return GL_STREAM_DRAW;
    default:
      return GL_STATIC_DRAW;
    }
  }
  [[nodiscard]] auto get_data_ptr() const -> void const* {
    if (source_type_ == SourceType::Data && !data_.empty()) {
      return data_.data();
    }
    return nullptr;
  }
};

} // namespace gl
