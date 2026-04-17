#pragma once

#include "rydz_graphics/gl/shader.hpp"
#include <concepts>
#include <string>
#include <unordered_map>
#include <utility>

namespace ecs {

using gl::ShaderSpec;
using gl::Uniform;

struct PostProcessDescriptor {
  ShaderSpec shader;
  std::unordered_map<std::string, Uniform> _uniforms;
  bool enabled = true;

  bool operator==(const PostProcessDescriptor &o) const = default;
};

template <typename M>
concept IsPostProcess = requires(const M &m) {
  { m.describe() } -> std::same_as<PostProcessDescriptor>;
};

struct PostProcessMaterial {
  PostProcessDescriptor material{};

  PostProcessMaterial() = default;
  explicit PostProcessMaterial(PostProcessDescriptor descriptor)
      : material(std::move(descriptor)) {}

  template <IsPostProcess M>
  explicit PostProcessMaterial(M material) : material(material.describe()) {}
};

struct DefaultPostProcessMaterial {
  bool enabled = true;
  float exposure = 1.0F;
  float contrast = 1.05F;
  float saturation = 1.0F;
  float vignette = 0.18F;
  float grain = 0.015F;

  [[nodiscard]]
  PostProcessDescriptor describe() const {
    PostProcessDescriptor descriptor;
    descriptor.shader = ShaderSpec::from("res/shaders/postprocess.vert",
                                         "res/shaders/postprocess.frag");
    descriptor.enabled = enabled;
    descriptor._uniforms.insert_or_assign("u_exposure", Uniform{exposure});
    descriptor._uniforms.insert_or_assign("u_contrast", Uniform{contrast});
    descriptor._uniforms.insert_or_assign("u_saturation", Uniform{saturation});
    descriptor._uniforms.insert_or_assign("u_vignette", Uniform{vignette});
    descriptor._uniforms.insert_or_assign("u_grain", Uniform{grain});
    return descriptor;
  }
};

} // namespace ecs
