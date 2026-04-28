#pragma once

#include "types.hpp"
#include <cstdint>
#include <cstdlib>
#include <ranges>
#include <string>
#include <type_traits>
#include <typeinfo>

// Symbol lookup helpers are not available on Windows builds in this codebase.
#if !defined(_WIN32)
#include <cxxabi.h>
#include <dlfcn.h>
#endif

namespace ecs {

inline auto demangle(char const* mangled) -> std::string {
#if defined(_MSC_VER)
  return mangled;
#elif defined(_WIN32)
  return mangled;
#else
  int status = 0;
  char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  if (status == 0 && (demangled != nullptr)) {
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
using copy_const_t = std::
  conditional_t<std::is_const_v<std::remove_reference_t<From>>, To const, To>;

template <typename Fn> auto system_name_of(Fn&& fn) -> std::string {
  using D = decay_t<Fn>;
  if constexpr (std::is_pointer_v<D> &&
                std::is_function_v<std::remove_pointer_t<D>>) {
#if defined(_MSC_VER) || defined(_WIN32)
    return std::to_string(reinterpret_cast<std::uintptr_t>(fn));
#else
    Dl_info info{};
    if (dladdr(reinterpret_cast<void const*>(fn), &info) != 0 &&
        info.dli_sname) {
      return demangle(info.dli_sname);
    }
    return std::to_string(reinterpret_cast<std::uintptr_t>(fn));
#endif
  } else {
    return demangle(typeid(D).name());
  }
}

inline constexpr auto range = std::ranges::views::iota;

} // namespace ecs
