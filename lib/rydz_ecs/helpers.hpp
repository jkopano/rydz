#pragma once

#include <type_traits>

template <typename From, typename To>
using copy_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>, const To,
                       To>;
