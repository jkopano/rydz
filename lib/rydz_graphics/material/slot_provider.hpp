#pragma once

#include "rydz_ecs/asset.hpp"
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

struct PreparedMaterial {
  std::array<gl::MaterialMap, K_MATERIAL_MAP_COUNT> local_maps{};
  gl::Material material;
};

struct SlotPrepareContext {
  Assets<Texture> const* texture_assets = nullptr;
  bool instanced = false;

  [[nodiscard]] auto textures() const -> Assets<Texture> const& {
    return *texture_assets;
  }
};

struct RenderSlotContext {
  ExtractedView const* view_data = nullptr;
  ExtractedLights const* lights_data = nullptr;
  ClusterConfig const* cluster_config_data = nullptr;
  ClusteredLightingState const* cluster_state_data = nullptr;
  bool instanced = false;

  [[nodiscard]] auto view() const -> ExtractedView const& {
    return *view_data;
  }
  [[nodiscard]] auto lights() const -> ExtractedLights const& {
    return *lights_data;
  }
  [[nodiscard]] auto cluster_config() const -> ClusterConfig const& {
    return *cluster_config_data;
  }
  [[nodiscard]] auto clustered_lighting() const
    -> ClusteredLightingState const& {
    return *cluster_state_data;
  }
};

struct SlotProvider {
  std::function<void(
    SlotPrepareContext const&,
    CompiledMaterial const&,
    PreparedMaterial&,
    ShaderProgram&
  )>
    prepare;
  std::function<void(
    RenderSlotContext const&,
    CompiledMaterial const&,
    PreparedMaterial const&,
    ShaderProgram&
  )>
    apply;
};

struct SlotProviderRegistry {
  using T = Resource;
  std::unordered_map<std::type_index, SlotProvider> providers;

  template <typename SlotT> void register_slot(SlotProvider provider) {
    this->providers[typeid(SlotT)] = std::move(provider);
  }
};

} // namespace ecs
