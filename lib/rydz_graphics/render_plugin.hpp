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

enum class RenderExtractSet {
  Extract,
  Queue,
  Prepare,
};

enum class RenderPassSet {
  Setup,
  Main,
  Overlay,
  Cleanup,
};

struct RenderPlugin {
  static void install(App &app) {
    app.init_resource<Assets<rl::Mesh>>(
           [](rl::Mesh &mesh) { rl::UnloadMesh(mesh); })
        .init_resource<Assets<rl::Texture2D>>(
            [](rl::Texture2D &texture) { rl::UnloadTexture(texture); })
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

    app.configure_set(ScheduleLabel::ExtractRender,
                      configure(RenderExtractSet::Queue)
                          .after(set(RenderExtractSet::Extract)))
        .configure_set(ScheduleLabel::ExtractRender,
                       configure(RenderExtractSet::Prepare)
                           .after(set(RenderExtractSet::Queue)))
        .configure_set(
            ScheduleLabel::Render,
            configure(RenderPassSet::Main).after(set(RenderPassSet::Setup)))
        .configure_set(
            ScheduleLabel::Render,
            configure(RenderPassSet::Overlay).after(set(RenderPassSet::Main)))
        .configure_set(ScheduleLabel::Render,
                       configure(RenderPassSet::Cleanup)
                           .after(set(RenderPassSet::Overlay)));

    app.add_systems(ScheduleLabel::First,
                     SceneRuntimeSystems::cleanup_orphan_scene_entities_system)
        .add_systems(ScheduleLabel::PreUpdate,
                     SceneRuntimeSystems::sync_scene_roots_system)

        .add_systems(ScheduleLabel::PostUpdate,
                     group(propagate_transforms, compute_visibility,
                           compute_mesh_bounds_system, frustum_cull_system))

        .add_systems(ScheduleLabel::ExtractRender,
                     group(extract_view_system, extract_lighting_system,
                           extract_meshes_system, extract_overlay_system)
                         .in_set(set(RenderExtractSet::Extract))
                         .chain())

        .add_systems(ScheduleLabel::ExtractRender,
                     group(RenderPhaseSystems::queue_shadow_phase,
                           RenderPhaseSystems::queue_opaque_phase,
                           RenderPhaseSystems::queue_transparent_phase,
                           RenderPhaseSystems::queue_overlay_phase)
                         .in_set(set(RenderExtractSet::Queue))
                         .chain())

        .add_systems(ScheduleLabel::ExtractRender,
                     group(RenderPhaseSystems::build_opaque_batches)
                         .in_set(set(RenderExtractSet::Prepare)))

        .add_systems(ScheduleLabel::Render,
                     group(RenderPassSystems::begin_frame)
                         .in_set(set(RenderPassSet::Setup)))

        .add_systems(ScheduleLabel::Render,
                     group(RenderPassSystems::run_shadow_pass,
                           RenderPassSystems::run_opaque_pass,
                           RenderPassSystems::run_transparent_pass)
                         .in_set(set(RenderPassSet::Main))
                         .chain())
        .add_systems(ScheduleLabel::Render,
                     group(RenderPassSystems::run_overlay_pass)
                         .in_set(set(RenderPassSet::Overlay)))
        .add_systems(ScheduleLabel::Render,
                     group(RenderPassSystems::end_frame)
                         .in_set(set(RenderPassSet::Cleanup)));
  }
};

inline void render_plugin(App &app) { RenderPlugin::install(app); }

} // namespace ecs
