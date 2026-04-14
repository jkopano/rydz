#pragma once

#include "clustered_lighting.hpp"
#include "frustum.hpp"
#include "render_extract.hpp"
#include "render_passes.hpp"
#include "render_phase.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/loaders.hpp"
#include "rydz_graphics/assets/scene_runtime.hpp"
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
  template <MaterialValue M> static void register_material(App &) {}

  template <typename SlotT>
  static void register_slot(App &app, SlotProvider provider) {
    app.init_resource<SlotProviderRegistry>();
    if (auto *registry = app.world().get_resource<SlotProviderRegistry>()) {
      registry->register_slot<SlotT>(std::move(provider));
    }
  }

  static void install(App &app) {
    app.init_resource<Assets<Mesh>>([](Mesh &mesh) { mesh.unload(); })
        .init_resource<Assets<Texture>>(
            [](Texture &texture) { texture.unload(); })
        .init_resource<Assets<Material>>()
        .init_resource<Assets<Scene>>()
        .init_resource<AssetServer>()
        .init_resource<ExtractedView>()
        .init_resource<ExtractedLights>()
        .init_resource<ExtractedMeshes>()
        .init_resource<ExtractedUi>()
        .init_resource<ClusterConfig>()
        .init_resource<ClusteredLightingState>()
        .init_resource<ShaderCache>()
        .init_resource<SlotProviderRegistry>()
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
    register_slot<HasCamera>(app, make_has_camera_slot_provider());
    register_slot<HasPBR>(app, make_has_pbr_slot_provider());

    app.add_systems(First, [](World &world) {
      if (auto *server = world.get_resource<AssetServer>()) {
        server->update(world);
      }
    });

    app.configure_set(ExtractRender, configure(RenderExtractSet::Extract,
                                               RenderExtractSet::Queue,
                                               RenderExtractSet::Prepare)
                                         .chain())
        .configure_set(Render,
                       configure(RenderPassSet::Setup, RenderPassSet::Main,
                                 RenderPassSet::PostProcess, RenderPassSet::Ui,
                                 RenderPassSet::Cleanup)
                           .chain());

    app.add_systems(First,
                    SceneRuntimeSystems::cleanup_orphan_scene_entities_system)

        .add_systems(PreUpdate, SceneRuntimeSystems::sync_scene_roots_system)

        .add_systems(PostUpdate,
                     group(propagate_transforms, compute_visibility,
                           compute_mesh_bounds_system, frustum_cull_system))

        .add_systems(RenderExtractSet::Extract,
                     group(extract::clear_meshes, extract::view,
                           extract::lighting, extract::ui, extract::meshes)
                         .chain())

        .add_systems(
            RenderExtractSet::Queue,
            group([](Res<ExtractedMeshes> m,
                     ResMut<ShadowPhase> p) { p->queue(*m); },
                  [](Res<ExtractedMeshes> m, ResMut<OpaquePhase> p) {
                    p->queue(*m);
                  },
                  [](Res<ExtractedMeshes> m, ResMut<TransparentPhase> p) {
                    p->queue(*m);
                  },
                  [](Res<ExtractedUi> u, ResMut<UiPhase> p) { p->queue(*u); })
                .chain())

        .add_systems(
            RenderExtractSet::Prepare,
            group([](ResMut<OpaquePhase> p, ResMut<Assets<Mesh>> a,
                     NonSendMarker) { p->build_batches(*a); },
                  [](ResMut<TransparentPhase> p, ResMut<Assets<Mesh>> a,
                     NonSendMarker) { p->build_batches(*a); })
                .chain())

        .add_systems(RenderPassSet::Setup, FramePass::begin)

        .add_systems(RenderPassSet::Main,
                     group(WorldPass::shadow, WorldPass::depth_prepass,
                           WorldPass::cluster_build, WorldPass::opaque,
                           WorldPass::transparent)
                         .chain())

        .add_systems(RenderPassSet::PostProcess, PostProcessPass::postprocess)

        .add_systems(RenderPassSet::Ui, UiPass::ui)
        .add_systems(RenderPassSet::Cleanup, FramePass::end);
  }
};

} // namespace ecs
