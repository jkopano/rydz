#pragma once

#include <ranges>
#include <type_traits>

using namespace std;

template <typename From, typename To>
using copy_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
                       To>;

namespace ecs {
namespace views = ranges::views;

inline constexpr auto range = ranges::views::iota;
} // namespace ecs
