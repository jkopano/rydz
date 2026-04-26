#pragma once

#include "rl.hpp"
#include "rydz_graphics/gl/buffers.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <external/glad.h>
#include <span>
#include <type_traits>

namespace gl {

enum class BufferUsage { Static, Dynamic, Stream };

template <typename T> class BufferBuilder {
  static_assert(
    std::is_trivially_copyable_v<T>, "Buffer element type must be trivially copyable"
  );

public:
  static auto with_data(std::span<T const> data) -> BufferBuilder<T>;
  static auto with_capacity(usize count) -> BufferBuilder<T>;

  auto dynamic(bool is_dynamic) -> BufferBuilder&;
  auto usage(BufferUsage usage_hint) -> BufferBuilder&;

  auto build_ssbo() -> SSBO;
  auto build_ubo() -> UBO;
  auto build_vbo() -> VertexBuffer;
  auto build_ebo() -> ElementBuffer;

private:
  enum class SourceType { None, Data, Capacity };

  SourceType source_type_ = SourceType::None;
  std::span<T const> data_;
  usize capacity_ = 0;
  BufferUsage usage_ = BufferUsage::Static;
  bool is_valid_ = true;

  BufferBuilder() = default;

  [[nodiscard]] auto calculate_size() const -> usize;
  [[nodiscard]] auto get_gl_usage() const -> u32;
  [[nodiscard]] auto get_data_ptr() const -> void const*;
};

template <typename T>
inline auto BufferBuilder<T>::with_data(std::span<T const> data) -> BufferBuilder<T> {
  BufferBuilder<T> builder;
  builder.source_type_ = SourceType::Data;
  builder.data_ = data;

  if (data.empty()) {
    builder.is_valid_ = false;
    warn("BufferBuilder::with_data: empty data span provided");
  }

  return builder;
}

template <typename T>
inline auto BufferBuilder<T>::with_capacity(usize count) -> BufferBuilder<T> {
  BufferBuilder<T> builder;
  builder.source_type_ = SourceType::Capacity;
  builder.capacity_ = count;

  if (count == 0) {
    builder.is_valid_ = false;
    warn("BufferBuilder::with_capacity: zero capacity provided");
  }

  return builder;
}

template <typename T>
inline auto BufferBuilder<T>::dynamic(bool is_dynamic) -> BufferBuilder& {
  usage_ = is_dynamic ? BufferUsage::Dynamic : BufferUsage::Static;
  return *this;
}

template <typename T>
inline auto BufferBuilder<T>::usage(BufferUsage usage_hint) -> BufferBuilder& {
  usage_ = usage_hint;
  return *this;
}

template <typename T> inline auto BufferBuilder<T>::build_ssbo() -> SSBO {
  if (!is_valid_) {
    warn("BufferBuilder::build_ssbo: builder is invalid, returning empty SSBO");
    return SSBO{};
  }

  usize const size = calculate_size();
  u32 const gl_usage = get_gl_usage();
  void const* data_ptr = get_data_ptr();

  return SSBO(static_cast<u32>(size), data_ptr, static_cast<i32>(gl_usage));
}

template <typename T> inline auto BufferBuilder<T>::build_ubo() -> UBO {
  if (!is_valid_) {
    warn("BufferBuilder::build_ubo: builder is invalid, returning empty UBO");
    return UBO{};
  }

  usize const size = calculate_size();
  u32 const gl_usage = get_gl_usage();
  void const* data_ptr = get_data_ptr();

  return UBO(static_cast<u32>(size), data_ptr, static_cast<i32>(gl_usage));
}

template <typename T> inline auto BufferBuilder<T>::build_vbo() -> VertexBuffer {
  if (!is_valid_) {
    warn("BufferBuilder::build_vbo: builder is invalid, returning empty VertexBuffer");
    return VertexBuffer{};
  }

  usize const size = calculate_size();
  void const* data_ptr = get_data_ptr();
  bool const is_dynamic =
    (usage_ == BufferUsage::Dynamic || usage_ == BufferUsage::Stream);

  return VertexBuffer(data_ptr, static_cast<i32>(size), is_dynamic);
}

template <typename T> inline auto BufferBuilder<T>::build_ebo() -> ElementBuffer {
  if (!is_valid_) {
    warn("BufferBuilder::build_ebo: builder is invalid, returning empty ElementBuffer");
    return ElementBuffer{};
  }

  usize const size = calculate_size();
  void const* data_ptr = get_data_ptr();
  bool const is_dynamic =
    (usage_ == BufferUsage::Dynamic || usage_ == BufferUsage::Stream);

  return ElementBuffer(data_ptr, static_cast<i32>(size), is_dynamic);
}

template <typename T> inline auto BufferBuilder<T>::calculate_size() const -> usize {
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

template <typename T> inline auto BufferBuilder<T>::get_gl_usage() const -> u32 {
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

template <typename T> inline auto BufferBuilder<T>::get_data_ptr() const -> void const* {
  if (source_type_ == SourceType::Data && !data_.empty()) {
    return data_.data();
  }
  return nullptr;
}

} // namespace gl
