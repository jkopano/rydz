#pragma once

#include "clustered_lighting.hpp"
#include "pipeline.hpp"
#include "render_extract.hpp"
#include "render_phase.hpp"
#include "rydz_ecs/core/time.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/render_config.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace ecs {

struct RenderExecutionState {
  using T = Resource;
  bool scene_active = false;
  bool world_pass_active = false;
  bool world_target_active = false;
  bool backbuffer_active = false;
};

struct FramePass {
  static auto begin(
    Res<ExtractedView> view,
    ResMut<RenderExecutionState> state,
    ResMut<PipelineState> screen_pipeline,
    Res<Window> window,
    NonSendMarker marker
  ) -> void;

  static auto end(
    ResMut<RenderExecutionState> state,
    ResMut<PipelineState> screen_pipeline,
    Res<DebugOverlaySettings> debug_settings,
    NonSendMarker marker
  ) -> void;
};

struct WorldPass {
  static auto shadow(Res<RenderExecutionState>, Res<ShadowPhase>, NonSendMarker)
    -> void {}

  static auto depth_prepass(
    Res<RenderExecutionState> state,
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

    RenderConfig::depth_prepass()(marker);
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
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
    RenderConfig::post_depth_prepass()(marker);
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

    RenderConfig::opaque()(marker);
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
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
    RenderConfig{}(marker);
  }

  static void transparent(
    Res<RenderExecutionState> state,
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

    RenderConfig::transparent()(marker);
    for (auto const& batch : phase->batches) {
      draw_batch_common(
        marker,
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
    RenderConfig{}(marker);
  }

  static auto begin(NonSendMarker marker, ExtractedView const& view) -> void {
    gl::begin_world_pass(view.camera_view.view, view.camera_view.proj);
    RenderConfig::get_default()(marker);

    if ((view.active_skybox != nullptr) && view.active_skybox->loaded) {
      view.active_skybox->draw(view.camera_view.view, view.camera_view.proj);
    }
  }

  static auto end(NonSendMarker marker) -> void {
    gl::end_world_pass();
    RenderConfig::end_world_pass()(marker);
  }

private:
  static auto depth_prepass_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const spec =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return spec;
  }

  template <typename BatchT>
  static auto draw_batch_common(
    NonSendMarker marker,
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
    batch.key.material.apply_cull_mode();
    draw_batch(shader, *mesh, prepared.material, batch);
  }
};

struct PostProcessPass {
  static auto postprocess(
    ResMut<RenderExecutionState> state,
    Res<PipelineState> pipeline,
    ResMut<ShaderCache> shader_cache,
    Res<ExtractedView> view,
    Res<Time> time,
    NonSendMarker marker
  ) -> void {
    if (state->world_pass_active) {
      WorldPass::end(marker);
      state->world_pass_active = false;
    }

    if (state->world_target_active) {
      pipeline->world_target.end();
      state->world_target_active = false;
    }

    if (!state->backbuffer_active) {
      gl::begin_drawing();
      gl::clear_background(gl::kBlack);
      state->backbuffer_active = true;
    }

    if (!state->scene_active || !pipeline->ready()) {
      return;
    }

    draw_postprocess_pass(
      marker, pipeline->world_target.texture, *shader_cache, *view, *time
    );
  }

private:
  static auto postprocess_source_rect(gl::Texture const& texture)
    -> gl::Rectangle {
    return {
      0.0F,
      0.0F,
      static_cast<f32>(texture.width),
      -static_cast<f32>(texture.height),
    };
  }

  static auto postprocess_dest_rect() -> gl::Rectangle {
    return {
      0.0F,
      0.0F,
      static_cast<f32>(gl::screen_width()),
      static_cast<f32>(gl::screen_height()),
    };
  }

  static auto draw_world_target_to_screen(gl::Texture const& source_texture)
    -> void {
    gl::draw_texture_pro(
      source_texture,
      postprocess_source_rect(source_texture),
      postprocess_dest_rect(),
      {0.0F, 0.0F},
      0.0F,
      gl::kWhite
    );
  }

  static auto apply_postprocess_uniforms(
    ShaderProgram& shader, PostProcessDescriptor const& effect, Time const& time
  ) -> void {
    gl::Vec2 const resolution = {
      static_cast<f32>(std::max(gl::screen_width(), 1)),
      static_cast<f32>(std::max(gl::screen_height(), 1)),
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
      draw_world_target_to_screen(source_texture);
      return;
    }

    ShaderProgram& shader =
      resolve_shader(marker, shader_cache, view.postprocess.shader);
    apply_postprocess_uniforms(shader, view.postprocess, time);
    shader.set_texture(MaterialMap::Albedo, source_texture);

    shader.with_bound([&] -> void {
      draw_world_target_to_screen(source_texture);
    });
  }
};

struct UiPass {
  static auto ui(
    Res<UiPhase> phase,
    Res<Assets<Texture>> texture_assets,
    ResMut<RenderExecutionState> state,
    NonSendMarker
  ) -> void {
    if (!state->backbuffer_active) {
      gl::begin_drawing();
      state->backbuffer_active = true;
    }

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
    return std::atan2(siny_cosp, cosy_cosp) * (180.0F / PI);
  }
};

inline auto FramePass::begin(
  Res<ExtractedView> view,
  ResMut<RenderExecutionState> state,
  ResMut<PipelineState> pipeline,
  Res<Window> window,
  NonSendMarker marker
) -> void {
  state->world_pass_active = false;
  state->world_target_active = false;
  state->backbuffer_active = false;
  state->scene_active = view->active;

  if (!view->active) {
    gl::begin_drawing();
    gl::clear_background(view->clear_color);
    state->backbuffer_active = true;
    return;
  }

  pipeline->ensure_target(window->width, window->height);
  pipeline->world_target.begin();
  gl::clear_background(view->clear_color);
  state->world_target_active = true;
  WorldPass::begin(marker, *view);
  state->world_pass_active = true;
}

inline auto FramePass::end(
  ResMut<RenderExecutionState> state,
  ResMut<PipelineState> pipeline,
  Res<DebugOverlaySettings> debug_settings,
  NonSendMarker marker
) -> void {
  if (state->world_pass_active) {
    WorldPass::end(marker);
    state->world_pass_active = false;
  }

  if (state->world_target_active) {
    pipeline->world_target.end();
    state->world_target_active = false;
  }

  if (!state->backbuffer_active) {
    gl::begin_drawing();
    state->backbuffer_active = true;
  }

  if (debug_settings->draw_fps) {
    gl::draw_fps(
      static_cast<i32>(debug_settings->fps_position.x),
      static_cast<i32>(debug_settings->fps_position.y)
    );
  }

  if (state->backbuffer_active) {
    gl::end_drawing();
    state->backbuffer_active = false;
  }
}

} // namespace ecs
