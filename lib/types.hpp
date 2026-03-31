#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

inline constexpr u8 U8_MAX = std::numeric_limits<u8>::max();
inline constexpr u16 U16_MAX = std::numeric_limits<u16>::max();
inline constexpr u32 U32_MAX = std::numeric_limits<u32>::max();
inline constexpr u64 U64_MAX = std::numeric_limits<u64>::max();

inline constexpr i8 I8_MAX = std::numeric_limits<i8>::max();
inline constexpr i16 I16_MAX = std::numeric_limits<i16>::max();
inline constexpr i32 I32_MAX = std::numeric_limits<i32>::max();
inline constexpr i64 I64_MAX = std::numeric_limits<i64>::max();

inline constexpr f32 F32_MAX = std::numeric_limits<f32>::max();
inline constexpr f64 F64_MAX = std::numeric_limits<f64>::max();

inline constexpr usize USIZE_MAX = std::numeric_limits<usize>::max();

template <typename... T> using Variant = std::variant<T...>;
template <typename... T> using Tuple = std::tuple<T...>;

template <class... Ts> struct over : Ts... {
  using Ts::operator()...;
};
template <class... Ts> over(Ts...) -> over<Ts...>;
