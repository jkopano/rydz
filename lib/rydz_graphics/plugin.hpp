#pragma once

#include "rydz_audio/plugin.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/assets/loaders.hpp"
#include "rydz_graphics/assets/scene_runtime.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/pipeline/config.hpp"
#include "rydz_graphics/pipeline/graph.hpp"
#include "rydz_graphics/pipeline/passes.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_graphics/spatial/frustum.hpp"

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

template <RenderMaterialAsset M> struct MaterialPlugin : IPlugin {
  void build(App& app) {
    app.init_resource<Assets<M>>();
    app.add_systems(
      RenderExtractSet::Extract,
      group(Extract::meshes<M>).after(Extract::clear_meshes).after(Extract::view)
    );
  }
};

struct RenderPlugin : IPlugin {

  template <typename SlotT>
  static auto register_slot(App& app, SlotProvider provider) -> void {
    app.init_resource<SlotProviderRegistry>();
    if (auto* registry = app.world().get_resource<SlotProviderRegistry>()) {
      registry->register_slot<SlotT>(std::move(provider));
    }
  }

  auto build(App& app) -> void {
    app.init_resource<Assets<Mesh>>([](Mesh& mesh) -> void { mesh.unload(); })
      .init_resource<Assets<Texture>>([](Texture& texture) -> void { texture.unload(); })
      .init_resource<Assets<Material>>()
      .init_resource<Assets<Scene>>()
      .init_resource<AssetServer>()
      .init_resource<ExtractedView>()
      .init_resource<ExtractedLights>()
      .init_resource<ExtractedMeshes>()
      .init_resource<ExtractedUi>()
      .init_resource<ClusterConfig>()
      .init_resource<ClusteredLightingState>()
      .init_resource<EnvironmentRenderer>()
      .init_resource<ShaderCache>()
      .init_resource<SlotProviderRegistry>()
      .init_resource<DebugOverlaySettings>()
      .init_resource<gl::RenderState>()
      .init_resource<ShadowPhase>()
      .init_resource<OpaquePhase>()
      .init_resource<TransparentPhase>()
      .init_resource<UiPhase>()
      .init_resource<RenderExecutionState>();

    app.init_resource<RenderGraph>();
    if (auto* graph = app.world().get_resource<RenderGraph>()) {
      auto const main_target = graph->create_texture({}, "MainTarget");
      auto const screen = graph->import_backbuffer("Screen");

      graph->add_pass<ClearPass>(main_target);
      graph->add_pass<ShadowPass>();
      graph->add_pass<DepthPrepass>(main_target);
      graph->add_pass<ClusterBuildPass>();
      graph->add_pass<EnvironmentPass>(main_target);
      graph->add_pass<OpaquePass>(main_target);
      graph->add_pass<TransparentPass>(main_target);
      graph->add_pass<PostProcessPassNode>(main_target, screen);
      graph->add_pass<UiPass>(main_target, screen);
    }

    if (auto* server = app.world().get_resource<AssetServer>()) {
      register_graphics_loaders(*server);
    }
    rydz_audio::AudioPlugin::install(app);
    register_slot<HasCamera>(app, HasCamera::slot_provider());
    register_slot<HasPBR>(app, HasPBR::slot_provider());

    app.add_systems(First, [](World& world) -> void {
      if (auto* server = world.get_resource<AssetServer>()) {
        server->update(world);
      }
    });

    app
      .configure_set(
        ExtractRender,
        configure(
          RenderExtractSet::Extract, RenderExtractSet::Queue, RenderExtractSet::Prepare
        )
          .chain()
      )
      .configure_set(
        Render,
        configure(RenderPassSet::Setup, RenderPassSet::Main, RenderPassSet::Cleanup)
          .chain()
      );

    app.add_systems(First, SceneRuntimeSystems::cleanup_orphan_scene_entities_system)
      .add_systems(Startup, initialize_environment_renderer)

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
        group(Queue::shadow, Queue::opaque, Queue::transparent, Queue::ui).chain()
      )

      .add_systems(RenderExtractSet::Prepare, group(Prepare::prepare_meshes))
      .add_systems(RenderPassSet::Setup, FramePass::begin)
      .add_systems(RenderPassSet::Main, FramePass::execute_graph)
      .add_systems(RenderPassSet::Cleanup, FramePass::end);

    MaterialPlugin<StandardMaterial>{}.build(app);
  }
};

} // namespace ecs
