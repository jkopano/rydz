#pragma once
#include "render_extract.hpp"
#include "render_material.hpp"
#include "render_phase.hpp"
#include <cmath>

namespace ecs {

struct RenderExecutionState {
  using T = Resource;
  bool world_pass_active = false;
};

inline void begin_world_pass(NonSendMarker, const ExtractedView &view) {
  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPushMatrix();
  rl::rlLoadIdentity();
  rl::rlSetMatrixProjection(to_rl(view.camera_view.proj));
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlSetMatrixModelview(to_rl(view.camera_view.view));
  rl::rlEnableDepthTest();

  if (view.has_render_config) {
    view.render_config.apply();
  }

  if (view.active_skybox && view.active_skybox->loaded) {
    view.active_skybox->draw(view.camera_view.view, view.camera_view.proj);
  }
}

inline void end_world_pass(NonSendMarker) {
  rl::rlDrawRenderBatchActive();
  rl::rlMatrixMode(RL_PROJECTION);
  rl::rlPopMatrix();
  rl::rlMatrixMode(RL_MODELVIEW);
  rl::rlLoadIdentity();
  rl::rlDisableDepthTest();
  rl::rlDisableBackfaceCulling();
}

inline void begin_frame_system(Res<ClearColor> clear_color,
                               Res<ExtractedView> view,
                               ResMut<RenderExecutionState> state,
                               NonSendMarker marker) {
  rl::BeginDrawing();
  rl::ClearBackground(clear_color->color);

  state->world_pass_active = false;
  if (!view->active) {
    return;
  }

  begin_world_pass(marker, *view);
  state->world_pass_active = true;
}

inline void run_shadow_pass_system(Res<RenderExecutionState>,
                                   Res<ShadowPhase>, NonSendMarker) {}

inline void
draw_opaque_batch(NonSendMarker marker, const OpaqueBatch &batch,
                  const Assets<rl::Mesh> &mesh_assets,
                  const Assets<rl::Texture2D> &texture_assets,
                  const ExtractedView &view, const ExtractedLights &lights) {
  const rl::Mesh *mesh = mesh_assets.get(batch.key.mesh);
  if (!mesh) {
    return;
  }

  PreparedMaterial prepared{};
  prepare_material(marker, batch.key.material, texture_assets, prepared);
  apply_shader_uniforms(marker, prepared.material.shader, prepared, view, lights);
  draw_batch_instances(*mesh, prepared.material, batch);
}

inline void run_opaque_pass_system(
    Res<RenderExecutionState> state, Res<OpaquePhase> phase,
    Res<Assets<rl::Mesh>> mesh_assets, Res<Assets<rl::Texture2D>> texture_assets,
    Res<ExtractedView> view, Res<ExtractedLights> lights, NonSendMarker marker) {
  if (!state->world_pass_active) {
    return;
  }

  for (const auto &batch : phase->batches) {
    draw_opaque_batch(marker, batch, *mesh_assets, *texture_assets, *view,
                      *lights);
  }
}

inline void draw_transparent_item(NonSendMarker marker,
                                  const TransparentPhaseItem &item,
                                  const Assets<rl::Mesh> &mesh_assets,
                                  const Assets<rl::Texture2D> &texture_assets,
                                  const ExtractedView &view,
                                  const ExtractedLights &lights) {
  const rl::Mesh *mesh = mesh_assets.get(item.mesh);
  if (!mesh) {
    return;
  }

  PreparedMaterial prepared{};
  prepare_material(marker, material_key(item.material), texture_assets,
                   prepared);
  apply_shader_uniforms(marker, prepared.material.shader, prepared, view, lights);
  rl::DrawMesh(*mesh, prepared.material, to_rl(item.world_transform));
}

inline void run_transparent_pass_system(
    Res<RenderExecutionState> state, Res<TransparentPhase> phase,
    Res<Assets<rl::Mesh>> mesh_assets, Res<Assets<rl::Texture2D>> texture_assets,
    Res<ExtractedView> view, Res<ExtractedLights> lights, NonSendMarker marker) {
  if (!state->world_pass_active) {
    return;
  }

  for (const auto &item : phase->items) {
    draw_transparent_item(marker, item, *mesh_assets, *texture_assets, *view,
                          *lights);
  }
}

inline float texture_rotation_degrees(const Transform &transform) {
  float siny_cosp = 2.0f * (transform.rotation.GetW() * transform.rotation.GetZ() +
                            transform.rotation.GetX() * transform.rotation.GetY());
  float cosy_cosp =
      1.0f - 2.0f * (transform.rotation.GetY() * transform.rotation.GetY() +
                     transform.rotation.GetZ() * transform.rotation.GetZ());
  return std::atan2(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
}

inline void run_overlay_pass_system(Res<OverlayPhase> phase,
                                    Res<Assets<rl::Texture2D>> texture_assets,
                                    ResMut<RenderExecutionState> state,
                                    NonSendMarker marker) {
  if (state->world_pass_active) {
    end_world_pass(marker);
    state->world_pass_active = false;
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
    rl::Rectangle dest = {position.x,
                          position.y,
                          texture->width * item.transform.scale.GetX(),
                          texture->height * item.transform.scale.GetY()};
    rl::Vector2 origin = {0, 0};

    rl::DrawTexturePro(*texture, source, dest, origin,
                       texture_rotation_degrees(item.transform), WHITE);
  }
}

inline void end_frame_system(ResMut<RenderExecutionState> state,
                             NonSendMarker marker) {
  if (state->world_pass_active) {
    end_world_pass(marker);
    state->world_pass_active = false;
  }

  rl::DrawFPS(10, 10);
  rl::EndDrawing();
}

} // namespace ecs
