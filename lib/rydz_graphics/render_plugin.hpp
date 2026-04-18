#pragma once

#include "clustered_lighting.hpp"
#include "frustum.hpp"
#include "pipeline.hpp"
#include "render_extract.hpp"
#include "render_passes.hpp"
#include "render_phase.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/loaders.hpp"
#include "rydz_graphics/assets/scene_runtime.hpp"

namespace ecs {

enum class RenderExtractSet : u8 {
  Extract,
  Queue,
  Prepare,
};

enum class RenderPassSet : u8 {
  Setup,
  Main,
  PostProcess,
  Ui,
  Cleanup,
};

struct RenderPlugin {
  template <RenderMaterialAsset M> static void register_material(App& app) {
    app.init_resource<Assets<M>>();
    app.add_systems(
      RenderExtractSet::Extract,
      group(Extract::meshes<M>)
        .after(Extract::clear_meshes)
        .after(Extract::view)
    );
  }

  template <typename SlotT>
  static auto register_slot(App& app, SlotProvider provider) -> void {
    app.init_resource<SlotProviderRegistry>();
    if (auto* registry = app.world().get_resource<SlotProviderRegistry>()) {
      registry->register_slot<SlotT>(std::move(provider));
    }
  }

  static auto install(App& app) -> void {
    app.init_resource<Assets<Mesh>>([](Mesh& mesh) -> void { mesh.unload(); })
      .init_resource<Assets<Texture>>([](Texture& texture) -> void {
        texture.unload();
      })
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
      .init_resource<PipelineState>()
      .init_resource<gl::RenderState>()
      .init_resource<ShadowPhase>()
      .init_resource<OpaquePhase>()
      .init_resource<TransparentPhase>()
      .init_resource<UiPhase>()
      .init_resource<RenderExecutionState>();

    if (auto* server = app.world().get_resource<AssetServer>()) {
      register_default_loaders(*server);
    }
    register_slot<HasCamera>(app, make_has_camera_slot_provider());
    register_slot<HasPBR>(app, make_has_pbr_slot_provider());

    app.add_systems(First, [](World& world) -> void {
      if (auto* server = world.get_resource<AssetServer>()) {
        server->update(world);
      }
    });

    app
      .configure_set(
        ExtractRender,
        configure(
          RenderExtractSet::Extract,
          RenderExtractSet::Queue,
          RenderExtractSet::Prepare
        )
          .chain()
      )
      .configure_set(
        Render,
        configure(
          RenderPassSet::Setup,
          RenderPassSet::Main,
          RenderPassSet::PostProcess,
          RenderPassSet::Ui,
          RenderPassSet::Cleanup
        )
          .chain()
      );

    app
      .add_systems(
        First, SceneRuntimeSystems::cleanup_orphan_scene_entities_system
      )

      .add_systems(PreUpdate, SceneRuntimeSystems::sync_scene_roots_system)

      .add_systems(
        PostUpdate,
        group(
          propagate_transforms,
          compute_visibility,
          compute_mesh_bounds_system,
          frustum_cull_system
        )
      );

    app
      .add_systems(
        RenderExtractSet::Extract,
        group(
          Extract::clear_meshes,
          Extract::view,
          Extract::lighting,
          Extract::ui,
          Extract::meshes<Material>
        )
          .chain()
      )

      .add_systems(
        RenderExtractSet::Queue,
        group(
          Queue::queue<ExtractedMeshes, ShadowPhase>,
          Queue::queue<ExtractedMeshes, OpaquePhase>,
          Queue::queue<ExtractedMeshes, TransparentPhase>,
          Queue::queue<ExtractedUi, UiPhase>
        )
          .chain()
      )

      .add_systems(
        RenderExtractSet::Prepare,
        group(
          Prepare::prepare_meshes,
          Prepare::build_batches<OpaquePhase>,
          Prepare::build_batches<TransparentPhase>
        )
          .chain()
      );

    app
      .add_systems(RenderPassSet::Setup, FramePass::begin)

      .add_systems(
        RenderPassSet::Main,
        group(
          WorldPass::shadow,
          WorldPass::depth_prepass,
          WorldPass::cluster_build,
          WorldPass::opaque,
          WorldPass::transparent
        )
          .chain()
      )

      .add_systems(RenderPassSet::PostProcess, PostProcessPass::postprocess)
      .add_systems(RenderPassSet::Ui, UiPass::ui)
      .add_systems(RenderPassSet::Cleanup, FramePass::end);

    register_material<StandardMaterial>(app);
  }
};

} // namespace ecs
