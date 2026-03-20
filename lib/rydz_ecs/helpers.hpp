#pragma once

#include "types.hpp"
#include <cstdint>
#include <ranges>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace ecs {

template <typename From, typename To>
using copy_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
                       To>;

template <typename Fn> std::string system_name_of(Fn &&fn) {
  using D = std::decay_t<Fn>;
  if constexpr (std::is_pointer_v<D> &&
                std::is_function_v<std::remove_pointer_t<D>>) {
    return std::to_string(reinterpret_cast<usize>(fn));
  } else {
    return typeid(D).name();
  }
}

namespace views = std::ranges::views;

template <typename T>
using bare_t = std::remove_cv_t<std::remove_reference_t<T>>;

inline constexpr auto range = std::ranges::views::iota;
} // namespace ecs
