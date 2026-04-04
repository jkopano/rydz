#pragma once
#include "clustered_lighting.hpp"
#include "frustum.hpp"
#include "render_extract.hpp"
#include "render_passes.hpp"
#include "render_phase.hpp"
#include "rl.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/asset_loaders.hpp"
#include "rydz_graphics/scene_runtime.hpp"
#include "screen_pipeline.hpp"

namespace ecs {

enum class RenderExtractSet {
  Extract,
  Queue,
  Prepare,
};

enum class RenderPassSet {
  Setup,
  Main,
  PostProcess,
  Ui,
  Cleanup,
};

struct RenderPlugin {
  template <IsMaterial M> static void register_material(App &app) {
    app.add_systems(
        RenderExtractSet::Extract,
        group(RenderExtractSystems::extract_meshes_system<M>)
            .after(RenderExtractSystems::clear_extracted_meshes_system));
  }

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
        .init_resource<ExtractedUi>()
        .init_resource<ClusterConfig>()
        .init_resource<ClusteredLightingState>()
        .init_resource<ShaderCache>()
        .init_resource<PostProcessSettings>()
        .init_resource<DebugOverlaySettings>()
        .init_resource<ScreenPipelineState>()
        .init_resource<ShadowPhase>()
        .init_resource<OpaquePhase>()
        .init_resource<TransparentPhase>()
        .init_resource<UiPhase>()
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
                      configure(RenderExtractSet::Extract,
                                RenderExtractSet::Queue,
                                RenderExtractSet::Prepare)
                          .chain())
        .configure_set(ScheduleLabel::Render,
                       configure(RenderPassSet::Setup, RenderPassSet::Main,
                                 RenderPassSet::PostProcess, RenderPassSet::Ui,
                                 RenderPassSet::Cleanup)
                           .chain());

    app.add_systems(ScheduleLabel::First,
                    SceneRuntimeSystems::cleanup_orphan_scene_entities_system)

        .add_systems(ScheduleLabel::PreUpdate,
                     SceneRuntimeSystems::sync_scene_roots_system)

        .add_systems(ScheduleLabel::PostUpdate,
                     group(propagate_transforms, compute_visibility,
                           compute_mesh_bounds_system, frustum_cull_system))

        .add_systems(RenderExtractSet::Extract,
                     group(RenderExtractSystems::clear_extracted_meshes_system,
                           RenderExtractSystems::extract_view_system,
                           RenderExtractSystems::extract_lighting_system,
                           RenderExtractSystems::extract_ui_system)
                         .chain())

        .add_systems(RenderExtractSet::Queue,
                     group(RenderPhaseSystems::queue_shadow_phase,
                           RenderPhaseSystems::queue_opaque_phase,
                           RenderPhaseSystems::queue_transparent_phase,
                           RenderPhaseSystems::queue_ui_phase)
                         .chain())

        .add_systems(RenderExtractSet::Prepare,
                     RenderPhaseSystems::build_opaque_batches)

        .add_systems(RenderPassSet::Setup, RenderPassSystems::begin_frame)

        .add_systems(RenderPassSet::Main,
                     group(RenderPassSystems::run_shadow_pass,
                           RenderPassSystems::run_depth_prepass,
                           RenderPassSystems::run_cluster_build_pass,
                           RenderPassSystems::run_opaque_pass,
                           RenderPassSystems::run_transparent_pass)
                         .chain())

        .add_systems(RenderPassSet::PostProcess,
                     RenderPassSystems::run_postprocess_pass)

        .add_systems(RenderPassSet::Ui, RenderPassSystems::run_ui_pass)
        .add_systems(RenderPassSet::Cleanup, RenderPassSystems::end_frame);

    register_material<StandardMaterial>(app);
  }
};

inline void render_plugin(App &app) { RenderPlugin::install(app); }

template <IsMaterial M> inline void render_material_plugin(App &app) {
  RenderPlugin::register_material<M>(app);
}

} // namespace ecs
