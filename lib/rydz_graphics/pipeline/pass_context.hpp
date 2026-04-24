#pragma once

#include "rydz_ecs/core/time.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/material/slot_provider.hpp"
#include "rydz_graphics/pipeline/phase.hpp"

namespace ecs {

struct RenderExecutionState;
struct EnvironmentRenderer;
struct ShaderCache;

struct PassContext {
  NonSendMarker marker;
  gl::RenderState& render_state;
  gl::Rectangle framebuffer;

  ExtractedView const& view;
  ExtractedLights const& lights;
  Time const& time;

  Assets<Mesh> const& mesh_assets;
  Assets<Texture> const& texture_assets;
  ShaderCache& shader_cache;
  SlotProviderRegistry const& slot_registry;

  OpaquePhase const& opaque_phase;
  TransparentPhase const& transparent_phase;
  ShadowPhase const& shadow_phase;
  UiPhase const& ui_phase;

  ClusterConfig const& cluster_config;
  ClusteredLightingState& cluster_state;

  RenderExecutionState* execution_state = nullptr;
  EnvironmentRenderer const* environment_renderer = nullptr;
};

} // namespace ecs
