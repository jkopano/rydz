#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <concepts>

namespace ecs {
template <typename C>
concept IComputeShader = requires {
  { C::vertex_source() } -> std::convertible_to<const char *>;
  { C::fragment_source() } -> std::convertible_to<const char *>;
  { C::shader() } -> std::convertible_to<const rl::Shader *>;
};

template <IComputeShader C> class ICompute {
  Handle<C> shader;
  virtual void apply();
};

} // namespace ecs
