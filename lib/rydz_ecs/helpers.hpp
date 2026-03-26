#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstdlib>
#include <ranges>
#include <string>
#include <type_traits>
#include <typeinfo>

// Sprawdzamy, czy nie jesteśmy na MSVC przed dołączeniem nagłówka
#if !defined(_MSC_VER)
#include <cxxabi.h>
#endif

namespace ecs {

inline std::string demangle(const char *mangled) {
#if defined(_MSC_VER)
  return mangled;
#else
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  if (status == 0 && demangled) {
    std::string result(demangled);
    std::free(demangled);
    return result;
  }
  return mangled;
#endif
}
template <typename T>
using bare_t = std::remove_cv_t<std::remove_reference_t<T>>;
template <typename T> using decay_t = std::decay_t<T>;

template <typename From, typename To>
using copy_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
                       To>;

template <typename Fn> std::string system_name_of(Fn &&fn) {
  using D = decay_t<Fn>;
  if constexpr (std::is_pointer_v<D> &&
                std::is_function_v<std::remove_pointer_t<D>>) {
    return std::to_string(reinterpret_cast<std::uintptr_t>(fn));
  } else {
    return demangle(typeid(D).name());
  }
}

namespace views = std::ranges::views;

inline constexpr auto range = std::ranges::views::iota;

} // namespace ecs
