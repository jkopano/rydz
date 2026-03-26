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

    inline std::string demangle(const char* mangled) {
        // Jeśli używamy kompilatora Microsoftu, po prostu zwracamy napis
#if defined(_MSC_VER)
        return mangled;
#else
    // Logika dla GCC/Clang
        int status = 0;
        char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::string result(demangled);
            std::free(demangled);
            return result;
        }
        return mangled;
#endif
    }

    template <typename From, typename To>
    using copy_const_t =
        std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
        To>;

    template <typename Fn>
    std::string system_name_of(Fn&& fn) {
        using D = std::decay_t<Fn>;
        if constexpr (std::is_pointer_v<D> &&
            std::is_function_v<std::remove_pointer_t<D>>) {
            // Zmieniamy reinterpret_cast na formatowanie hex, aby łatwiej czytać w konsoli
            return std::to_string(reinterpret_cast<std::uintptr_t>(fn));
        }
        else {
            // Używamy naszej nowej, bezpiecznej funkcji demangle
            return demangle(typeid(D).name());
        }
    }

    namespace views = std::ranges::views;
    template <typename T>
    using bare_t = std::remove_cv_t<std::remove_reference_t<T>>;

    inline constexpr auto range = std::ranges::views::iota;

} // namespace ecs