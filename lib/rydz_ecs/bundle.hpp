#pragma once
#include "world.hpp"
#include <tuple>

namespace ecs {

template <typename T>
void insert_bundle(World &world, Entity entity, T &&component) {
  world.insert_component(entity, std::forward<T>(component));
}

template <typename... Ts>
void insert_bundle(World &world, Entity entity, std::tuple<Ts...> &&bundle) {
  std::apply(
      [&](auto &&...components) {
        (world.insert_component(entity,
                                std::forward<decltype(components)>(components)),
         ...);
      },
      std::move(bundle));
}

template <typename... Ts>
void insert_bundle(World &world, Entity entity,
                   const std::tuple<Ts...> &bundle) {
  std::apply(
      [&](const auto &...components) {
        (world.insert_component(entity, components), ...);
      },
      bundle);
}

} // namespace ecs
