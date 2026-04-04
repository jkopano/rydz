#pragma once

#include "clustered_lighting.hpp"
#include "render_extract.hpp"
#include "render_material.hpp"
#include "render_phase.hpp"
#include "rydz_ecs/core/time.hpp"
#include "screen_pipeline.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace ecs {

struct RenderExecutionState {
  using T = Resource;
  bool scene_active = false;
  bool world_pass_active = false;
  bool world_target_active = false;
  bool backbuffer_active = false;
};

struct RenderPassSystems {
  struct Frame {
    static void begin_frame(Res<ExtractedView> view,
                            ResMut<RenderExecutionState> state,
                            ResMut<ScreenPipelineState> screen_pipeline,
                            NonSendMarker marker) {
      state->world_pass_active = false;
      state->world_target_active = false;
      state->backbuffer_active = false;
      state->scene_active = view->active;

      if (!view->active) {
        rl::BeginDrawing();
        rl::ClearBackground(view->clear_color);
        state->backbuffer_active = true;
        return;
      }

      screen_pipeline->ensure_target(rl::GetScreenWidth(),
                                     rl::GetScreenHeight());
      rl::BeginTextureMode(screen_pipeline->world_target);
      rl::ClearBackground(view->clear_color);
      state->world_target_active = true;
      World::begin_world_pass(marker, *view);
      state->world_pass_active = true;
    }

    static void end_frame(ResMut<RenderExecutionState> state,
                          Res<DebugOverlaySettings> debug_settings,
                          NonSendMarker marker) {
      if (state->world_pass_active) {
        World::end_world_pass(marker);
        state->world_pass_active = false;
      }

      if (state->world_target_active) {
        rl::EndTextureMode();
        state->world_target_active = false;
      }

      if (!state->backbuffer_active) {
        rl::BeginDrawing();
        state->backbuffer_active = true;
      }

      if (debug_settings->draw_fps) {
        rl::DrawFPS(static_cast<int>(debug_settings->fps_position.x),
                    static_cast<int>(debug_settings->fps_position.y));
      }

      if (state->backbuffer_active) {
        rl::EndDrawing();
        state->backbuffer_active = false;
      }
    }
  };

  struct World {
    static void run_shadow_pass(Res<RenderExecutionState>, Res<ShadowPhase>,
                                NonSendMarker) {}

    static void run_depth_prepass(Res<RenderExecutionState> state,
                                  Res<OpaquePhase> phase,
                                  Res<Assets<rl::Mesh>> mesh_assets,
                                  Res<Assets<rl::Texture2D>> texture_assets,
                                  ResMut<ShaderCache> shader_cache,
                                  Res<ExtractedView> view,
                                  NonSendMarker marker) {
      if (!state->world_pass_active) {
        return;
      }

      begin_depth_prepass(marker);
      for (const auto &batch : phase->batches) {
        draw_depth_batch(marker, batch, *mesh_assets, *texture_assets,
                         *shader_cache);
      }
      end_depth_prepass(marker, *view);
    }

    static void run_cluster_build_pass(
        Res<RenderExecutionState> state, Res<ExtractedView> view,
        Res<ExtractedLights> lights, Res<ClusterConfig> cluster_config,
        ResMut<ClusteredLightingState> cluster_state, NonSendMarker marker) {
      if (!state->world_pass_active) {
        return;
      }

      build_cluster_buffers(marker, *view, *lights, *cluster_config,
                            *cluster_state);
    }

    static void run_opaque_pass(
        Res<RenderExecutionState> state, Res<OpaquePhase> phase,
        Res<Assets<rl::Mesh>> mesh_assets,
        Res<Assets<rl::Texture2D>> texture_assets,
        ResMut<ShaderCache> shader_cache, Res<ExtractedView> view,
        Res<ExtractedLights> lights, Res<ClusterConfig> cluster_config,
        Res<ClusteredLightingState> cluster_state, NonSendMarker marker) {
      if (!state->world_pass_active) {
        return;
      }

      for (const auto &batch : phase->batches) {
        draw_opaque_batch(marker, batch, *mesh_assets, *texture_assets,
                          *shader_cache, *view, *lights, *cluster_config,
                          *cluster_state);
      }
    }

    static void run_transparent_pass(
        Res<RenderExecutionState> state, Res<TransparentPhase> phase,
        Res<Assets<rl::Mesh>> mesh_assets,
        Res<Assets<rl::Texture2D>> texture_assets,
        ResMut<ShaderCache> shader_cache, Res<ExtractedView> view,
        Res<ExtractedLights> lights, Res<ClusterConfig> cluster_config,
        Res<ClusteredLightingState> cluster_state, NonSendMarker marker) {
      if (!state->world_pass_active) {
        return;
      }

      for (const auto &item : phase->items) {
        draw_transparent_item(marker, item, *mesh_assets, *texture_assets,
                              *shader_cache, *view, *lights, *cluster_config,
                              *cluster_state);
      }
    }

    static void begin_world_pass(NonSendMarker, const ExtractedView &view) {
      rl::rlDrawRenderBatchActive();
      rl::rlMatrixMode(RL_PROJECTION);
      rl::rlPushMatrix();
      rl::rlLoadIdentity();
      rl::rlSetMatrixProjection(to_rl(view.camera_view.proj));
      rl::rlMatrixMode(RL_MODELVIEW);
      rl::rlLoadIdentity();
      rl::rlSetMatrixModelview(to_rl(view.camera_view.view));
      RenderConfig::restore_defaults();

      if (view.has_render_config) {
        view.render_config.apply();
      }

      if (view.active_skybox && view.active_skybox->loaded) {
        view.active_skybox->draw(view.camera_view.view, view.camera_view.proj);
      }
    }

    static void end_world_pass(NonSendMarker) {
      rl::rlDrawRenderBatchActive();
      rl::rlMatrixMode(RL_PROJECTION);
      rl::rlPopMatrix();
      rl::rlMatrixMode(RL_MODELVIEW);
      rl::rlLoadIdentity();
      rl::rlDisableDepthTest();
      rl::rlEnableDepthMask();
      rl::rlDisableBackfaceCulling();
      rl::rlDisableWireMode();
      rl::rlSetBlendMode(RL_BLEND_ALPHA);
    }

  private:
    static const ShaderSpec &depth_prepass_shader_spec() {
      static const ShaderSpec spec =
          ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
      return spec;
    }

    static void begin_depth_prepass(NonSendMarker) {
      rl::rlDrawRenderBatchActive();
      rl::rlColorMask(false, false, false, false);
      rl::rlDisableColorBlend();
      rl::rlEnableDepthTest();
      rl::rlEnableDepthMask();
      rl::rlEnableBackfaceCulling();
      rl::rlSetCullFace(RL_CULL_FACE_BACK);
    }

    static void end_depth_prepass(NonSendMarker, const ExtractedView &view) {
      rl::rlDrawRenderBatchActive();
      rl::rlColorMask(true, true, true, true);
      rl::rlEnableColorBlend();
      if (view.has_render_config) {
        view.render_config.apply();
      } else {
        RenderConfig::restore_defaults();
      }
    }

    static void draw_depth_batch(NonSendMarker marker, const OpaqueBatch &batch,
                                 const Assets<rl::Mesh> &mesh_assets,
                                 const Assets<rl::Texture2D> &texture_assets,
                                 ShaderCache &shader_cache) {
      const rl::Mesh *mesh = mesh_assets.get(batch.key.mesh);
      if (!mesh) {
        return;
      }

      PreparedMaterial prepared{};
      prepare_material(marker, batch.key.material, texture_assets, shader_cache,
                       prepared);
      ShaderProgram &depth_shader =
          resolve_shader(marker, shader_cache, depth_prepass_shader_spec());
      prepared.material.shader = depth_shader.raw();

      float alpha_cutoff = 0.1f;
      depth_shader.set("u_alpha_cutoff", alpha_cutoff);
      draw_batch_instances(*mesh, prepared.material, batch);
    }

    static void build_cluster_buffers(NonSendMarker, const ExtractedView &view,
                                      const ExtractedLights &lights,
                                      const ClusterConfig &config,
                                      ClusteredLightingState &state) {
      static u32 last_reported_overflow = U32_MAX;

      state.ensure_buffers(config);
      state.reset_cpu_storage(config);

      const usize point_light_count =
          std::min(lights.point_lights.size(),
                   static_cast<usize>(config.max_point_lights));
      if (lights.point_lights.size() > point_light_count) {
        rl::TraceLog(
            LOG_WARNING,
            "Forward+: dropping %d point lights beyond configured cap",
            static_cast<int>(lights.point_lights.size() - point_light_count));
      }

      for (usize i = 0; i < point_light_count; ++i) {
        const auto &light = lights.point_lights[i];
        state.point_lights_cpu.push_back(GpuPointLight{
            .position_range = {light.position.x, light.position.y,
                               light.position.z, light.range},
            .color_intensity = {light.color.r / 255.0f, light.color.g / 255.0f,
                                light.color.b / 255.0f, light.intensity},
        });
      }

      const Mat4 inverse_projection = view.camera_view.proj.Inversed();
      for (i32 z = 0; z < config.slice_count_z; ++z) {
        for (i32 y = 0; y < config.tile_count_y; ++y) {
          for (i32 x = 0; x < config.tile_count_x; ++x) {
            const u32 cluster_index = static_cast<u32>(
                (z * config.tile_count_y + y) * config.tile_count_x + x);
            state.clusters_cpu[cluster_index] = build_cluster_record(
                config, inverse_projection, view.orthographic, view.near_plane,
                view.far_plane, x, y, z);
          }
        }
      }

      u32 overflow_count = 0;

      for (u32 light_index = 0;
           light_index < static_cast<u32>(state.point_lights_cpu.size());
           ++light_index) {
        const auto &light = state.point_lights_cpu[light_index];
        const Vec3 view_position_math =
            view.camera_view.view * Vec3(light.position_range.x,
                                         light.position_range.y,
                                         light.position_range.z);
        const rl::Vector3 view_position = to_rl(view_position_math);

        for (u32 cluster_index = 0;
             cluster_index < static_cast<u32>(state.clusters_cpu.size());
             ++cluster_index) {
          auto &cluster = state.clusters_cpu[cluster_index];
          if (!sphere_intersects_cluster(view_position, light.position_range.w,
                                         cluster)) {
            continue;
          }

          if (cluster.meta[1] >= config.max_lights_per_cluster) {
            ++overflow_count;
            continue;
          }

          const u32 offset = cluster.meta[0] + cluster.meta[1];
          state.light_indices_cpu[offset] = light_index;
          ++cluster.meta[1];
        }
      }

      if (!state.point_lights_cpu.empty()) {
        rl::rlUpdateShaderBuffer(
            state.point_light_buffer, state.point_lights_cpu.data(),
            static_cast<unsigned int>(state.point_lights_cpu.size() *
                                      sizeof(GpuPointLight)),
            0);
      }
      rl::rlUpdateShaderBuffer(
          state.cluster_buffer, state.clusters_cpu.data(),
          static_cast<unsigned int>(state.clusters_cpu.size() *
                                    sizeof(ClusterGpuRecord)),
          0);
      rl::rlUpdateShaderBuffer(
          state.light_index_buffer, state.light_indices_cpu.data(),
          static_cast<unsigned int>(state.light_indices_cpu.size() *
                                    sizeof(u32)),
          0);
      rl::rlUpdateShaderBuffer(state.overflow_buffer, &overflow_count,
                               sizeof(overflow_count), 0);

      if (overflow_count > 0) {
        if (last_reported_overflow != overflow_count) {
          rl::TraceLog(LOG_WARNING,
                       "Forward+: cluster light list overflowed %u writes",
                       overflow_count);
          last_reported_overflow = overflow_count;
        }
      } else {
        last_reported_overflow = U32_MAX;
      }
    }

    static void draw_opaque_batch(NonSendMarker marker,
                                  const OpaqueBatch &batch,
                                  const Assets<rl::Mesh> &mesh_assets,
                                  const Assets<rl::Texture2D> &texture_assets,
                                  ShaderCache &shader_cache,
                                  const ExtractedView &view,
                                  const ExtractedLights &lights,
                                  const ClusterConfig &cluster_config,
                                  const ClusteredLightingState &cluster_state) {
      const rl::Mesh *mesh = mesh_assets.get(batch.key.mesh);
      if (!mesh) {
        return;
      }

      PreparedMaterial prepared{};
      prepare_material(marker, batch.key.material, texture_assets, shader_cache,
                       prepared);
      ShaderProgram &shader =
          resolve_shader(marker, shader_cache, batch.key.material.shader);
      apply_shader_uniforms(marker, shader, batch.key.material, prepared, view,
                            lights, cluster_config, cluster_state);
      draw_batch_instances(*mesh, prepared.material, batch);
    }

    static void draw_transparent_item(
        NonSendMarker marker, const TransparentPhaseItem &item,
        const Assets<rl::Mesh> &mesh_assets,
        const Assets<rl::Texture2D> &texture_assets, ShaderCache &shader_cache,
        const ExtractedView &view, const ExtractedLights &lights,
        const ClusterConfig &cluster_config,
        const ClusteredLightingState &cluster_state) {
      const rl::Mesh *mesh = mesh_assets.get(item.mesh);
      if (!mesh) {
        return;
      }

      PreparedMaterial prepared{};
      prepare_material(marker, item.material, texture_assets, shader_cache,
                       prepared);
      ShaderProgram &shader =
          resolve_shader(marker, shader_cache, item.material.shader);
      apply_shader_uniforms(marker, shader, item.material, prepared, view,
                            lights, cluster_config, cluster_state);
      rl::DrawMesh(*mesh, prepared.material, to_rl(item.world_transform));
    }

    static bool sphere_intersects_cluster(const rl::Vector3 &center,
                                          float radius,
                                          const ClusterGpuRecord &cluster) {
      const float closest_x =
          std::clamp(center.x, cluster.min_bounds.x, cluster.max_bounds.x);
      const float closest_y =
          std::clamp(center.y, cluster.min_bounds.y, cluster.max_bounds.y);
      const float closest_z =
          std::clamp(center.z, cluster.min_bounds.z, cluster.max_bounds.z);
      const float dx = center.x - closest_x;
      const float dy = center.y - closest_y;
      const float dz = center.z - closest_z;
      return dx * dx + dy * dy + dz * dz <= radius * radius;
    }
  };

  struct PostProcessing {
    static void run_postprocess_pass(ResMut<RenderExecutionState> state,
                                     Res<ScreenPipelineState> screen_pipeline,
                                     ResMut<ShaderCache> shader_cache,
                                     Res<ExtractedView> view, Res<Time> time,
                                     NonSendMarker marker) {
      if (state->world_pass_active) {
        World::end_world_pass(marker);
        state->world_pass_active = false;
      }

      if (state->world_target_active) {
        rl::EndTextureMode();
        state->world_target_active = false;
      }

      if (!state->backbuffer_active) {
        rl::BeginDrawing();
        rl::ClearBackground(BLACK);
        state->backbuffer_active = true;
      }

      if (!state->scene_active || !screen_pipeline->ready()) {
        return;
      }

      draw_postprocess_pass(marker, screen_pipeline->world_target.texture,
                            *shader_cache, *view, *time);
    }

  private:
    static rl::Rectangle postprocess_source_rect(
        const rl::Texture2D &texture) {
      return {
          0.0f,
          0.0f,
          static_cast<float>(texture.width),
          -static_cast<float>(texture.height),
      };
    }

    static rl::Rectangle postprocess_dest_rect() {
      return {
          0.0f,
          0.0f,
          static_cast<float>(rl::GetScreenWidth()),
          static_cast<float>(rl::GetScreenHeight()),
      };
    }

    static void draw_world_target_to_screen(
        const rl::Texture2D &source_texture) {
      rl::DrawTexturePro(source_texture,
                         postprocess_source_rect(source_texture),
                         postprocess_dest_rect(), {0.0f, 0.0f}, 0.0f, WHITE);
    }

    static void apply_postprocess_uniforms(ShaderProgram &shader,
                                           const PostProcessDescriptor &effect,
                                           const Time &time) {
      const rl::Vector2 resolution = {
          static_cast<float>(std::max(rl::GetScreenWidth(), 1)),
          static_cast<float>(std::max(rl::GetScreenHeight(), 1)),
      };
      shader.set("u_resolution", resolution);
      shader.set("u_time", time.elapsed_seconds);
      shader.set("u_enabled", effect.enabled ? 1 : 0);
      for (const auto &uniform : effect.uniforms) {
        shader.apply(uniform);
      }
    }

    static void draw_postprocess_pass(NonSendMarker marker,
                                      const rl::Texture2D &source_texture,
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
      shader.set_texture(shader.raw().locs[SHADER_LOC_MAP_DIFFUSE],
                         source_texture);

      shader.with_bound([&] { draw_world_target_to_screen(source_texture); });
    }
  };

  struct Ui {
    static void run_ui_pass(Res<UiPhase> phase,
                            Res<Assets<rl::Texture2D>> texture_assets,
                            ResMut<RenderExecutionState> state,
                            NonSendMarker) {
      if (!state->backbuffer_active) {
        rl::BeginDrawing();
        state->backbuffer_active = true;
      }

      for (const auto &item : phase->items) {
        const rl::Texture2D *texture = texture_assets->get(item.texture);
        if (!texture) {
          continue;
        }

        rl::Vector2 position = {item.transform.translation.GetX(),
                                item.transform.translation.GetY()};
        rl::Rectangle source = {0, 0, static_cast<float>(texture->width),
                                static_cast<float>(texture->height)};
        rl::Rectangle dest = {position.x, position.y,
                              texture->width * item.transform.scale.GetX(),
                              texture->height * item.transform.scale.GetY()};
        rl::Vector2 origin = {0, 0};

        rl::DrawTexturePro(*texture, source, dest, origin,
                           texture_rotation_degrees(item.transform), item.tint);
      }
    }

  private:
    static float texture_rotation_degrees(const Transform &transform) {
      float siny_cosp =
          2.0f * (transform.rotation.GetW() * transform.rotation.GetZ() +
                  transform.rotation.GetX() * transform.rotation.GetY());
      float cosy_cosp =
          1.0f - 2.0f * (transform.rotation.GetY() * transform.rotation.GetY() +
                         transform.rotation.GetZ() * transform.rotation.GetZ());
      return std::atan2(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
    }
  };
};

} // namespace ecs
