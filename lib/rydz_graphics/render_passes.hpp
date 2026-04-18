#pragma once

#include "clustered_lighting.hpp"
#include "pipeline.hpp"
#include "render_extract.hpp"
#include "render_phase.hpp"
#include "rl.hpp"
#include "rydz_ecs/core/time.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/render_config.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace ecs {

struct RenderExecutionState {
  using T = Resource;
  bool scene_active = false;
  bool world_pass_active = false;
  bool world_target_active = false;
  bool backbuffer_active = false;

  auto begin() -> void { rl::BeginDrawing(); };
  auto end() -> void { rl::EndDrawing(); };
  auto clear(Color color) -> void { rl::ClearBackground(color); };
};

struct FramePass {
  static auto begin(
    Res<ExtractedView> view,
    ResMut<RenderExecutionState> state,
    ResMut<PipelineState> screen_pipeline,
    ResMut<gl::RenderState> render_state,
    NonSendMarker marker
  ) -> void;

  static auto end(
    ResMut<RenderExecutionState> state,
    ResMut<PipelineState> screen_pipeline,
    ResMut<gl::RenderState> render_state,
    Res<DebugOverlaySettings> debug_settings,
    NonSendMarker marker
  ) -> void;
};

struct WorldPass {
  static auto shadow(
    Res<RenderExecutionState>,
    Res<ShadowPhase>,
    ResMut<gl::RenderState>,
    NonSendMarker
  ) -> void {}

  static auto depth_prepass(
    Res<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    Res<OpaquePhase> phase,
    Res<Assets<Mesh>> mesh_assets,
    Res<Assets<Texture>> texture_assets,
    ResMut<ShaderCache> shader_cache,
    Res<SlotProviderRegistry> slot_registry,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ClusterConfig> cluster_config,
    Res<ClusteredLightingState> cluster_state,
    NonSendMarker marker
  ) -> void {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig const config = RenderConfig::depth_prepass();
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
        *render_state,
        config,
        batch,
        *mesh_assets,
        *texture_assets,
        *shader_cache,
        *slot_registry,
        *view,
        *lights,
        *cluster_config,
        *cluster_state,
        depth_prepass_shader_spec()
      );
    }
    render_state->apply(RenderConfig::post_depth_prepass());
  }

  static auto cluster_build(
    Res<RenderExecutionState> state,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ClusterConfig> cluster_config,
    ResMut<ClusteredLightingState> cluster_state,
    NonSendMarker marker
  ) -> void {
    if (!state->world_pass_active) {
      return;
    }

    cluster_state->build_cluster_buffers(
      marker, *view, *lights, *cluster_config
    );
  }

  static void opaque(
    Res<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    Res<OpaquePhase> phase,
    Res<Assets<Mesh>> mesh_assets,
    Res<Assets<Texture>> texture_assets,
    ResMut<ShaderCache> shader_cache,
    Res<SlotProviderRegistry> slot_registry,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ClusterConfig> cluster_config,
    Res<ClusteredLightingState> cluster_state,
    NonSendMarker marker
  ) {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig const config = RenderConfig::opaque();
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
        *render_state,
        config,
        batch,
        *mesh_assets,
        *texture_assets,
        *shader_cache,
        *slot_registry,
        *view,
        *lights,
        *cluster_config,
        *cluster_state,
        batch.key.material.shader
      );
    }
    render_state->apply(RenderConfig{});
  }

  static void transparent(
    Res<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    Res<TransparentPhase> phase,
    Res<Assets<Mesh>> mesh_assets,
    Res<Assets<Texture>> texture_assets,
    ResMut<ShaderCache> shader_cache,
    Res<SlotProviderRegistry> slot_registry,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ClusterConfig> cluster_config,
    Res<ClusteredLightingState> cluster_state,
    NonSendMarker marker
  ) {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig const config = RenderConfig::transparent();
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
        *render_state,
        config,
        batch,
        *mesh_assets,
        *texture_assets,
        *shader_cache,
        *slot_registry,
        *view,
        *lights,
        *cluster_config,
        *cluster_state,
        batch.key.material.shader
      );
    }
    render_state->apply(RenderConfig{});
  }

  static auto begin(
    NonSendMarker marker,
    gl::RenderState& render_state,
    ExtractedView const& view
  ) -> void {
    (void) marker;

    render_state.begin_view(
      gl::RenderViewState{
        .viewport = view.viewport,
        .view = view.camera_view.view,
        .projection = view.camera_view.proj,
        .camera_position = view.camera_view.position,
        .orthographic = view.orthographic,
        .near_plane = view.near_plane,
        .far_plane = view.far_plane,
      }
    );

    render_state.apply(RenderConfig::get_default());

    if ((view.active_skybox != nullptr) && view.active_skybox->loaded) {
      auto const& active_view = render_state.view();
      view.active_skybox->draw(active_view.view, active_view.projection);
      render_state.reset();
    }
  }

  static auto end(NonSendMarker marker, gl::RenderState& render_state) -> void {
    (void) marker;
    render_state.end_view();
    render_state.apply(RenderConfig{});
  }

private:
  static auto depth_prepass_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return SPEC;
  }

  template <typename BatchT>
  static auto draw_batch_common(
    NonSendMarker marker,
    gl::RenderState& render_state,
    RenderConfig pass_config,
    BatchT const& batch,
    Assets<Mesh> const& mesh_assets,
    Assets<Texture> const& texture_assets,
    ShaderCache& shader_cache,
    SlotProviderRegistry const& slot_registry,
    ExtractedView const& view,
    ExtractedLights const& lights,
    ClusterConfig const& cluster_config,
    ClusteredLightingState const& cluster_state,
    ShaderSpec const& shader_spec
  ) -> void {
    auto const* mesh = mesh_assets.get(batch.key.mesh);
    if (!mesh) {
      return;
    }

    pass_config.cull = batch.key.material.cull_state();
    render_state.apply(pass_config);

    PreparedMaterial prepared{};
    SlotPrepareContext prepare_ctx{
      .texture_assets = &texture_assets,
      .instanced = true,
    };
    ShaderProgram& shader = prepare_material(
      marker,
      batch.key.material,
      texture_assets,
      shader_cache,
      slot_registry,
      prepare_ctx,
      shader_spec,
      prepared
    );
    batch.key.material.apply(shader);
    RenderSlotContext render_ctx{
      .view_data = &view,
      .lights_data = &lights,
      .cluster_config_data = &cluster_config,
      .cluster_state_data = &cluster_state,
      .instanced = true,
    };
    apply_slot_uniforms(
      slot_registry, render_ctx, batch.key.material, prepared, shader
    );
    draw_batch(shader, *mesh, prepared.material, batch);
  }
};

struct PostProcessPass {
  static auto postprocess(
    ResMut<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    Res<PipelineState> pipeline,
    ResMut<ShaderCache> shader_cache,
    Res<ExtractedView> view,
    Res<Time> time,
    NonSendMarker marker
  ) -> void {
    if (state->world_pass_active) {
      WorldPass::end(marker, *render_state);
      state->world_pass_active = false;
    }

    if (state->world_target_active) {
      render_state->end_target();
      state->world_target_active = false;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->clear(Color::BLACK);
      state->backbuffer_active = true;
    }

    if (!state->scene_active || !pipeline->ready()) {
      return;
    }

    draw_postprocess_pass(
      marker, pipeline->world_target.texture, *shader_cache, *view, *time
    );
    render_state->reset();
  }

private:
  static auto draw_world_target_to_screen(
    gl::Texture const& source_texture, ExtractedView const& view
  ) -> void {
    gl::draw_texture_pro(
      source_texture,
      source_texture.rect().flipped_y(),
      view.viewport,
      {0.0F, 0.0F},
      0.0F,
      Color::WHITE
    );
  }

  static auto apply_postprocess_uniforms(
    ShaderProgram& shader,
    PostProcessDescriptor const& effect,
    ExtractedView const& view,
    Time const& time
  ) -> void {
    gl::Vec2 const resolution = {
      std::max(view.viewport.width, 1.0F),
      std::max(view.viewport.height, 1.0F),
    };
    shader.set("u_resolution", resolution);
    shader.set("u_time", time.elapsed_seconds);
    shader.set("u_enabled", effect.enabled ? 1 : 0);
    for (auto const& [name, uniform] : effect._uniforms) {
      shader.apply(name, uniform);
    }
  }

  static auto draw_postprocess_pass(
    NonSendMarker marker,
    gl::Texture const& source_texture,
    ShaderCache& shader_cache,
    ExtractedView const& view,
    Time const& time
  ) -> void {
    if (!view.has_postprocess || !view.postprocess.enabled) {
      draw_world_target_to_screen(source_texture, view);
      return;
    }

    ShaderProgram& shader =
      resolve_shader(marker, shader_cache, view.postprocess.shader);
    apply_postprocess_uniforms(shader, view.postprocess, view, time);
    shader.set_texture(MaterialMap::Albedo, source_texture);

    shader.with_bound([&] -> void {
      draw_world_target_to_screen(source_texture, view);
    });
  }
};

struct UiPass {
  static auto ui(
    Res<UiPhase> phase,
    Res<Assets<Texture>> texture_assets,
    ResMut<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    NonSendMarker
  ) -> void {
    if (!state->backbuffer_active) {
      state->begin();
      state->backbuffer_active = true;
    }

    render_state->apply(RenderConfig{});

    for (auto const& item : phase->items) {
      auto const* texture = texture_assets->get(item.texture);
      if (texture == nullptr) {
        continue;
      }

      gl::Vec2 position = {
        item.transform.translation.x, item.transform.translation.y
      };
      gl::Rectangle source = {
        0,
        0,
        static_cast<f32>(texture->width),
        static_cast<f32>(texture->height)
      };
      gl::Rectangle dest = {
        position.x,
        position.y,
        texture->width * item.transform.scale.x,
        texture->height * item.transform.scale.y
      };
      gl::Vec2 origin = {0, 0};

      gl::draw_texture_pro(
        *texture,
        source,
        dest,
        origin,
        texture_rotation_degrees(item.transform),
        item.tint
      );
    }
  }

private:
  static auto texture_rotation_degrees(Transform const& transform) -> f32 {
    f32 siny_cosp = 2.0F * (transform.rotation.w * transform.rotation.z +
                            transform.rotation.x * transform.rotation.y);
    f32 cosy_cosp =
      1.0F - (2.0F * (transform.rotation.y * transform.rotation.y +
                      transform.rotation.z * transform.rotation.z));
    return std::atan2(siny_cosp, cosy_cosp) *
           (180.0F / std::numbers::pi_v<float>);
  }
};

inline auto FramePass::begin(
  Res<ExtractedView> view,
  ResMut<RenderExecutionState> state,
  ResMut<PipelineState> pipeline,
  ResMut<gl::RenderState> render_state,
  NonSendMarker marker
) -> void {
  render_state->reset();
  state->world_pass_active = false;
  state->world_target_active = false;
  state->backbuffer_active = false;
  state->scene_active = view->active;

  if (!view->active) {
    state->begin();
    state->clear(view->clear_color);
    state->backbuffer_active = true;
    return;
  }

  pipeline->ensure_target(
    static_cast<u32>(std::max(view->viewport.width, 1.0F)),
    static_cast<u32>(std::max(view->viewport.height, 1.0F))
  );
  render_state->begin_target(pipeline->world_target);
  state->clear(view->clear_color);
  state->world_target_active = true;
  WorldPass::begin(marker, *render_state, *view);
  state->world_pass_active = true;
}

inline auto FramePass::end(
  ResMut<RenderExecutionState> state,
  ResMut<PipelineState> pipeline,
  ResMut<gl::RenderState> render_state,
  Res<DebugOverlaySettings> debug_settings,
  NonSendMarker marker
) -> void {
  if (state->world_pass_active) {
    WorldPass::end(marker, *render_state);
    state->world_pass_active = false;
  }

  if (state->world_target_active) {
    render_state->end_target();
    state->world_target_active = false;
  }

  if (!state->backbuffer_active) {
    state->begin();
    state->backbuffer_active = true;
  }

  if (debug_settings->draw_fps) {
    gl::draw_fps(
      static_cast<i32>(debug_settings->fps_position.x),
      static_cast<i32>(debug_settings->fps_position.y)
    );
  }

  if (state->backbuffer_active) {
    state->end();
    state->backbuffer_active = false;
  }
}

} // namespace ecs
