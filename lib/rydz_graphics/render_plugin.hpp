#pragma once
#include "frustum.hpp"
#include "render_extract.hpp"
#include "render_passes.hpp"
#include "render_phase.hpp"
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/asset_loaders.hpp"
#include "rydz_graphics/scene_runtime.hpp"

namespace ecs {

struct RenderPlugin {
  static void install(App &app) {
    app.init_resource<Assets<rl::Mesh>>([](rl::Mesh &mesh) {
           rl::UnloadMesh(mesh);
         })
        .init_resource<Assets<rl::Texture2D>>([](rl::Texture2D &texture) {
          rl::UnloadTexture(texture);
        })
        .init_resource<Assets<Scene>>()
        .init_resource<AssetServer>()
        .init_resource<ClearColor>()
        .init_resource<ExtractedView>()
        .init_resource<ExtractedLights>()
        .init_resource<ExtractedMeshes>()
        .init_resource<ExtractedOverlay>()
        .init_resource<ShadowPhase>()
        .init_resource<OpaquePhase>()
        .init_resource<TransparentPhase>()
        .init_resource<OverlayPhase>()
        .init_resource<RenderExecutionState>();

    if (auto *server = app.world().get_resource<AssetServer>()) {
      register_default_loaders(*server);
    }

    app.add_systems(ScheduleLabel::First, [](World &world) {
      if (auto *server = world.get_resource<AssetServer>()) {
        server->update(world);
      }
    });

    app.add_systems(ScheduleLabel::First, cleanup_orphan_scene_entities_system)
        .add_systems(ScheduleLabel::PreUpdate, sync_scene_roots_system)

        .add_systems(ScheduleLabel::PostUpdate,
                     group(propagate_transforms, compute_visibility,
                           compute_mesh_bounds_system, frustum_cull_system))

        .add_systems(ScheduleLabel::ExtractRender, extract_view_system)
        .add_systems(ScheduleLabel::ExtractRender, extract_lighting_system)
        .add_systems(ScheduleLabel::ExtractRender, extract_meshes_system)
        .add_systems(ScheduleLabel::ExtractRender, extract_overlay_system)
        .add_systems(ScheduleLabel::ExtractRender, queue_shadow_phase_system)
        .add_systems(ScheduleLabel::ExtractRender, queue_opaque_phase_system)
        .add_systems(ScheduleLabel::ExtractRender,
                     queue_transparent_phase_system)
        .add_systems(ScheduleLabel::ExtractRender, queue_overlay_phase_system)
        .add_systems(ScheduleLabel::ExtractRender, build_opaque_batches_system)

        .add_systems(ScheduleLabel::Render, begin_frame_system)
        .add_systems(ScheduleLabel::Render, run_shadow_pass_system)
        .add_systems(ScheduleLabel::Render, run_opaque_pass_system)
        .add_systems(ScheduleLabel::Render, run_transparent_pass_system)
        .add_systems(ScheduleLabel::Render, run_overlay_pass_system)
        .add_systems(ScheduleLabel::Render, end_frame_system);
  }
};

inline void render_plugin(App &app) { RenderPlugin::install(app); }

} // namespace ecs
