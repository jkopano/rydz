#pragma once

#include "rydz_ecs/core/time.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/material/material_slot.hpp"
#include "rydz_graphics/pipeline/config.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_graphics/shadow.hpp"

namespace ecs {

struct EnvironmentRenderer;
struct RenderExecutionState;
struct ShaderCache;
struct ViewUniformState;

struct PassContext {
  NonSendMarker marker;
  gl::RenderState& render_state;
  gl::Rectangle framebuffer;
  ExtractedView const& view;
  ExtractedLights const& lights;
  ExtractedShadows const& shadows;
  Time const& time;
  ExtractedMeshes const& extracted_meshes;
  Assets<Mesh> const& mesh_assets;
  Assets<Texture> const& texture_assets;
  ShaderCache& shader_cache;
  ViewUniformState& view_uniforms;
  ShadowUniformState& shadow_uniforms;
  SlotProviderRegistry const& slot_registry;
  OpaquePhase const& opaque_phase;
  TransparentPhase const& transparent_phase;
  ShadowPhase const& shadow_phase;
  UiPhase const& ui_phase;
  ClusterConfig const& cluster_config;
  ShadowSettings const& shadow_settings;
  ClusteredLightingState& cluster_state;
  ShadowResources& shadow_resources;
  RenderExecutionState* execution_state{};
  EnvironmentRenderer const* environment_renderer{};
};

} // namespace ecs
