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
  static void begin(Res<ExtractedView> view, ResMut<RenderExecutionState> state,
                    ResMut<PipelineState> screen_pipeline,
                    NonSendMarker marker);

  static void end(ResMut<RenderExecutionState> state,
                  ResMut<PipelineState> screen_pipeline,
                  Res<DebugOverlaySettings> debug_settings,
                  NonSendMarker marker);
};

struct WorldPass {
  static void shadow(Res<RenderExecutionState>, Res<ShadowPhase>,
                     NonSendMarker) {}

  static void depth_prepass(
      Res<RenderExecutionState> state, Res<OpaquePhase> phase,
      Res<Assets<Mesh>> mesh_assets, Res<Assets<Texture>> texture_assets,
      ResMut<ShaderCache> shader_cache, Res<SlotProviderRegistry> slot_registry,
      Res<ExtractedView> view, Res<ExtractedLights> lights,
      Res<ClusterConfig> cluster_config,
      Res<ClusteredLightingState> cluster_state, NonSendMarker marker) {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig::depth_prepass()(marker);
    for (const auto &batch : phase->batches) {
      draw_batch_common(marker, batch, *mesh_assets, *texture_assets,
                        *shader_cache, *slot_registry, *view, *lights,
                        *cluster_config, *cluster_state,
                        depth_prepass_shader_spec());
    }
    RenderConfig::post_depth_prepass()(marker);
  }

  static void cluster_build(Res<RenderExecutionState> state,
                            Res<ExtractedView> view,
                            Res<ExtractedLights> lights,
                            Res<ClusterConfig> cluster_config,
                            ResMut<ClusteredLightingState> cluster_state,
                            NonSendMarker marker) {
    if (!state->world_pass_active) {
      return;
    }

    cluster_state->build_cluster_buffers(marker, *view, *lights,
                                         *cluster_config);
  }

  static void
  opaque(Res<RenderExecutionState> state, Res<OpaquePhase> phase,
         Res<Assets<Mesh>> mesh_assets, Res<Assets<Texture>> texture_assets,
         ResMut<ShaderCache> shader_cache,
         Res<SlotProviderRegistry> slot_registry, Res<ExtractedView> view,
         Res<ExtractedLights> lights, Res<ClusterConfig> cluster_config,
         Res<ClusteredLightingState> cluster_state, NonSendMarker marker) {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig::opaque()(marker);
    for (const auto &batch : phase->batches) {
      draw_batch_common(marker, batch, *mesh_assets, *texture_assets,
                        *shader_cache, *slot_registry, *view, *lights,
                        *cluster_config, *cluster_state,
                        batch.key.material.shader);
    }
    RenderConfig{}(marker);
  }

  static void
  transparent(Res<RenderExecutionState> state, Res<TransparentPhase> phase,
              Res<Assets<Mesh>> mesh_assets,
              Res<Assets<Texture>> texture_assets,
              ResMut<ShaderCache> shader_cache,
              Res<SlotProviderRegistry> slot_registry, Res<ExtractedView> view,
              Res<ExtractedLights> lights, Res<ClusterConfig> cluster_config,
              Res<ClusteredLightingState> cluster_state, NonSendMarker marker) {
    if (!state->world_pass_active) {
      return;
    }

    RenderConfig::transparent()(marker);
    for (const auto &batch : phase->batches) {
      draw_batch_common(marker, batch, *mesh_assets, *texture_assets,
                        *shader_cache, *slot_registry, *view, *lights,
                        *cluster_config, *cluster_state,
                        batch.key.material.shader);
    }
    RenderConfig{}(marker);
  }

  static void begin(NonSendMarker marker, const ExtractedView &view) {
    gl::begin_world_pass(view.camera_view.view, view.camera_view.proj);
    RenderConfig::get_default()(marker);

    if ((view.active_skybox != nullptr) && view.active_skybox->loaded) {
      view.active_skybox->draw(view.camera_view.view, view.camera_view.proj);
    }
  }

  static void end(NonSendMarker marker) {
    gl::end_world_pass();
    RenderConfig::end_world_pass()(marker);
  }

private:
  static const ShaderSpec &depth_prepass_shader_spec() {
    static const ShaderSpec spec =
        ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return spec;
  }

  template <typename BatchT>
  static void draw_batch_common(NonSendMarker marker, const BatchT &batch,
                                const Assets<Mesh> &mesh_assets,
                                const Assets<Texture> &texture_assets,
                                ShaderCache &shader_cache,
                                const SlotProviderRegistry &slot_registry,
                                const ExtractedView &view,
                                const ExtractedLights &lights,
                                const ClusterConfig &cluster_config,
                                const ClusteredLightingState &cluster_state,
                                const ShaderSpec &shader_spec) {
    const auto *mesh = mesh_assets.get(batch.key.mesh);
    if (!mesh) {
      return;
    }

    PreparedMaterial prepared{};
    SlotPrepareContext prepare_ctx{
        .texture_assets = &texture_assets,
        .instanced = true,
    };
    ShaderProgram &shader = prepare_material(
        marker, batch.key.material, texture_assets, shader_cache, slot_registry,
        prepare_ctx, shader_spec, prepared);
    batch.key.material.apply(shader);
    RenderSlotContext render_ctx{
        .view_data = &view,
        .lights_data = &lights,
        .cluster_config_data = &cluster_config,
        .cluster_state_data = &cluster_state,
        .instanced = true,
    };
    apply_slot_uniforms(slot_registry, render_ctx, batch.key.material, prepared,
                        shader);
    batch.key.material.apply_cull_mode();
    draw_batch(shader, *mesh, prepared.material, batch);
  }
};

struct PostProcessPass {
  static void postprocess(ResMut<RenderExecutionState> state,
                          Res<PipelineState> pipeline,
                          ResMut<ShaderCache> shader_cache,
                          Res<ExtractedView> view, Res<Time> time,
                          NonSendMarker marker) {
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

    draw_postprocess_pass(marker, pipeline->world_target.texture, *shader_cache,
                          *view, *time);
  }

private:
  static gl::Rectangle postprocess_source_rect(const gl::Texture &texture) {
    return {
        0.0F,
        0.0F,
        static_cast<f32>(texture.width),
        -static_cast<f32>(texture.height),
    };
  }

  static gl::Rectangle postprocess_dest_rect() {
    return {
        0.0F,
        0.0F,
        static_cast<f32>(gl::screen_width()),
        static_cast<f32>(gl::screen_height()),
    };
  }

  static void draw_world_target_to_screen(const gl::Texture &source_texture) {
    gl::draw_texture_pro(
        source_texture, postprocess_source_rect(source_texture),
        postprocess_dest_rect(), {0.0F, 0.0F}, 0.0F, gl::kWhite);
  }

  static void apply_postprocess_uniforms(ShaderProgram &shader,
                                         const PostProcessDescriptor &effect,
                                         const Time &time) {
    const gl::Vec2 resolution = {
        static_cast<f32>(std::max(gl::screen_width(), 1)),
        static_cast<f32>(std::max(gl::screen_height(), 1)),
    };
    shader.set("u_resolution", resolution);
    shader.set("u_time", time.elapsed_seconds);
    shader.set("u_enabled", effect.enabled ? 1 : 0);
    for (const auto &[name, uniform] : effect._uniforms) {
      shader.apply(std::string(name), uniform);
    }
  }

  static void draw_postprocess_pass(NonSendMarker marker,
                                    const gl::Texture &source_texture,
                                    ShaderCache &shader_cache,
                                    const ExtractedView &view,
                                    const Time &time) {
    if (!view.has_postprocess || !view.postprocess.enabled) {
      draw_world_target_to_screen(source_texture);
      return;
    }

    ShaderProgram &shader =
        resolve_shader(marker, shader_cache, view.postprocess.shader);
    apply_postprocess_uniforms(shader, view.postprocess, time);
    shader.set_texture(shader.raw().locs[gl::SHADER_LOC_MAP_DIFFUSE],
                       source_texture);

    shader.with_bound([&] { draw_world_target_to_screen(source_texture); });
  }
};

struct UiPass {
  static void ui(Res<UiPhase> phase, Res<Assets<Texture>> texture_assets,
                 ResMut<RenderExecutionState> state, NonSendMarker) {
    if (!state->backbuffer_active) {
      gl::begin_drawing();
      state->backbuffer_active = true;
    }

    for (const auto &item : phase->items) {
      const auto *texture = texture_assets->get(item.texture);
      if (texture == nullptr) {
        continue;
      }

      gl::Vec2 position = {item.transform.translation.GetX(),
                           item.transform.translation.GetY()};
      gl::Rectangle source = {0, 0, static_cast<f32>(texture->width),
                              static_cast<f32>(texture->height)};
      gl::Rectangle dest = {position.x, position.y,
                            texture->width * item.transform.scale.GetX(),
                            texture->height * item.transform.scale.GetY()};
      gl::Vec2 origin = {0, 0};

      gl::draw_texture_pro(*texture, source, dest, origin,
                           texture_rotation_degrees(item.transform), item.tint);
    }
  }

private:
  static f32 texture_rotation_degrees(const Transform &transform) {
    f32 siny_cosp =
        2.0F * (transform.rotation.GetW() * transform.rotation.GetZ() +
                transform.rotation.GetX() * transform.rotation.GetY());
    f32 cosy_cosp =
        1.0f - 2.0f * (transform.rotation.GetY() * transform.rotation.GetY() +
                       transform.rotation.GetZ() * transform.rotation.GetZ());
    return std::atan2(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
  }
};

inline void FramePass::begin(Res<ExtractedView> view,
                             ResMut<RenderExecutionState> state,
                             ResMut<PipelineState> pipeline,
                             NonSendMarker marker) {
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

  pipeline->ensure_target(gl::screen_width(), gl::screen_height());
  pipeline->world_target.begin();
  gl::clear_background(view->clear_color);
  state->world_target_active = true;
  WorldPass::begin(marker, *view);
  state->world_pass_active = true;
}

inline void FramePass::end(ResMut<RenderExecutionState> state,
                           ResMut<PipelineState> pipeline,
                           Res<DebugOverlaySettings> debug_settings,
                           NonSendMarker marker) {
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
    gl::draw_fps(static_cast<i32>(debug_settings->fps_position.x),
                 static_cast<i32>(debug_settings->fps_position.y));
  }

  if (state->backbuffer_active) {
    gl::end_drawing();
    state->backbuffer_active = false;
  }
}

} // namespace ecs
