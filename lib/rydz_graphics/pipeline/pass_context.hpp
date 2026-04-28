#pragma once

#include "rydz_ecs/core/time.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/lighting/shadow.hpp"
#include "rydz_graphics/material/material_slot.hpp"
#include "rydz_graphics/pipeline/config.hpp"
#include "rydz_graphics/pipeline/phase.hpp"

namespace ecs {

struct EnvironmentRenderer;
struct RenderExecutionState;
struct ShaderCache;
struct ViewUniformState;

struct FrameContext {
  NonSendMarker marker;
  gl::RenderState& render_state;
  gl::Rectangle framebuffer;
  ExtractedView const& view;
  Time const& time;
  gl::Texture const* scene_depth_texture{};
  RenderExecutionState* execution_state{};
  EnvironmentRenderer const* environment_renderer{};
};

struct AssetContext {
  ExtractedMeshes const& extracted_meshes;
  Assets<Mesh> const& mesh_assets;
  Assets<Texture> const& texture_assets;
  ShaderCache& shader_cache;
  SlotProviderRegistry const& slot_registry;
};

struct LightingContext {
  ExtractedLights const& lights;
  ExtractedShadows const& shadows;
  ClusterConfig const& cluster_config;
  ShadowSettings const& shadow_settings;
  ClusteredLightingState& cluster_state;
  ShadowResources& shadow_resources;
  ViewUniformState& view_uniforms;
  ShadowUniformState& shadow_uniforms;
};

struct PhaseContext {
  OpaquePhase const& opaque;
  TransparentPhase const& transparent;
  ShadowPhase const& shadow;
  UiPhase const& ui;
};

struct PassContext {
  FrameContext frame;
  AssetContext assets;
  LightingContext lighting;
  PhaseContext phases;
};

} // namespace ecs
