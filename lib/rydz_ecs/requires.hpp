#pragma once
#include <tuple>

namespace ecs {

template <typename... Ts> struct Requires {
  using as_tuple = std::tuple<Ts...>;
};

template <typename T>
concept HasRequired = requires { typename T::Required; };

} // namespace ecs
