#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <concepts>

namespace ecs {

template <typename C>
concept IComputeShader = requires {
  { C::vertex_source() } -> std::convertible_to<char const*>;
  { C::fragment_source() } -> std::convertible_to<char const*>;
  { C::shader() } -> std::convertible_to<gl::Shader const*>;
};

template <IComputeShader C> class ICompute {
  Handle<C> shader;
  virtual auto apply() -> void;
};

} // namespace ecs
