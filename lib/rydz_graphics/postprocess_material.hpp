#pragma once

#include "shader.hpp"
#include <concepts>
#include <utility>
#include <vector>

namespace ecs {

struct PostProcessDescriptor {
  ShaderSpec shader;
  std::vector<Uniform> uniforms;
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
  explicit PostProcessMaterial(M material)
      : material(material.describe()) {}
};

struct DefaultPostProcessMaterial {
  bool enabled = true;
  float exposure = 1.0f;
  float contrast = 1.05f;
  float saturation = 1.0f;
  float vignette = 0.18f;
  float grain = 0.015f;

  PostProcessDescriptor describe() const {
    PostProcessDescriptor descriptor;
    descriptor.shader =
        ShaderSpec::from("res/shaders/postprocess.vert",
                         "res/shaders/postprocess.frag");
    descriptor.enabled = enabled;
    descriptor.uniforms.push_back(Uniform::float1("u_exposure", exposure));
    descriptor.uniforms.push_back(Uniform::float1("u_contrast", contrast));
    descriptor.uniforms.push_back(Uniform::float1("u_saturation", saturation));
    descriptor.uniforms.push_back(Uniform::float1("u_vignette", vignette));
    descriptor.uniforms.push_back(Uniform::float1("u_grain", grain));
    return descriptor;
  }
};

} // namespace ecs
