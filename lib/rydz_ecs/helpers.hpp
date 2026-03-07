#pragma once

#include <type_traits>

using namespace std;

template <typename From, typename To>
using copy_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
                       To>;

namespace ecs {
namespace views = std::ranges::views;

inline constexpr auto range = std::ranges::views::iota;
} // namespace ecs
