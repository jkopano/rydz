#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/core/time.hpp"
#include "rydz_graphics/gl/mesh.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include <array>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace gl {
struct ClusterConfig;
struct ClusteredLightingState;
} // namespace gl

namespace ecs {

using gl::ClusterConfig;
using gl::ClusteredLightingState;
using gl::ShaderProgram;
using gl::Texture;

struct CompiledMaterial;
struct ExtractedLights;
struct ExtractedView;
struct PassContext;
struct SlotProviderRegistry;
struct ViewUniformState;

struct PreparedMaterial {
  std::array<gl::MaterialMap, K_MATERIAL_MAP_COUNT> local_maps{};
  gl::Material material;
};

struct MaterialContext {
  PassContext const* frame_data = nullptr;
  bool instanced = false;

  [[nodiscard]] auto pass_ctx() const -> PassContext const&;
  [[nodiscard]] auto textures() const -> Assets<Texture> const&;
  [[nodiscard]] auto view() const -> ExtractedView const&;
  [[nodiscard]] auto lights() const -> ExtractedLights const&;
  [[nodiscard]] auto cluster_config() const -> ClusterConfig const&;
  [[nodiscard]] auto clustered_lighting() const -> ClusteredLightingState const&;
  [[nodiscard]] auto time() const -> Time const&;
  [[nodiscard]] auto view_uniforms() const -> ViewUniformState const&;
  [[nodiscard]] auto slots() const -> SlotProviderRegistry const&;
};

struct SlotProvider {
  std::function<void(
    MaterialContext const&, CompiledMaterial const&, PreparedMaterial&, ShaderProgram&
  )>
    prepare;
  std::function<void(MaterialContext const&, ShaderProgram&)> apply_per_view;
  std::function<void(
    MaterialContext const&,
    CompiledMaterial const&,
    PreparedMaterial const&,
    ShaderProgram&
  )>
    apply_per_material;
};

struct SlotProviderRegistry {
  using T = Resource;
  std::unordered_map<std::type_index, SlotProvider> providers;

  template <typename SlotT> void register_slot(SlotProvider provider) {
    this->providers[typeid(SlotT)] = std::move(provider);
  }
};

} // namespace ecs
