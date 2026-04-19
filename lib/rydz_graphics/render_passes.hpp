#pragma once

#include "clustered_lighting.hpp"
#include "pipeline.hpp"
#include "render_extract.hpp"
#include "render_graph.hpp"
#include "render_phase.hpp"
#include "rl.hpp"
#include "rydz_ecs/core/time.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/render_config.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace ecs {

inline constexpr char const* K_MAIN_TARGET = "MainTarget";

struct RenderExecutionState {
  using T = Resource;
  bool scene_active = false;
  bool world_pass_active = false;
  bool world_target_active = false;
  bool backbuffer_active = false;

  auto begin() -> void { rl::BeginDrawing(); };
  auto end() -> void { rl::EndDrawing(); };
  auto clear(Color color) -> void { rl::ClearBackground(color); };
};

struct RenderContext {
  NonSendMarker marker;
  gl::RenderState& render_state;
  Assets<Mesh> const& mesh_assets;
  Assets<Texture> const& texture_assets;
  ShaderCache& shader_cache;
  SlotProviderRegistry const& slot_registry;
  ExtractedView const& view;
  ExtractedLights const& lights;
  ClusterConfig const& cluster_config;
  ClusteredLightingState const& cluster_state;

  RenderConfig pass_config{};
  ShaderProgram* last_shader = nullptr;
  CompiledMaterial const* last_material = nullptr;
  PreparedMaterial last_prepared{};
  RenderSlotContext render_slot_ctx;

  RenderContext(
    NonSendMarker marker,
    gl::RenderState& render_state,
    Assets<Mesh> const& mesh_assets,
    Assets<Texture> const& texture_assets,
    ShaderCache& shader_cache,
    SlotProviderRegistry const& slot_registry,
    ExtractedView const& view,
    ExtractedLights const& lights,
    ClusterConfig const& cluster_config,
    ClusteredLightingState const& cluster_state
  )
      : marker(marker), render_state(render_state), mesh_assets(mesh_assets),
        texture_assets(texture_assets), shader_cache(shader_cache),
        slot_registry(slot_registry), view(view), lights(lights),
        cluster_config(cluster_config), cluster_state(cluster_state),
        render_slot_ctx{
          .view_data = &view,
          .lights_data = &lights,
          .cluster_config_data = &cluster_config,
          .cluster_state_data = &cluster_state,
          .instanced = true
        } {}

  auto begin_pass(RenderConfig const& config) -> void {
    pass_config = config;
    last_shader = nullptr;
    last_material = nullptr;
    last_prepared = {};
    render_state.apply(pass_config);
  }

  auto draw_command(RenderCommand const& cmd, ShaderSpec const& shader_spec)
    -> void {
    auto const* mesh = mesh_assets.get(cmd.mesh);
    if (!mesh) {
      return;
    }

    pass_config.cull = cmd.material.cull_state();
    render_state.apply(pass_config);

    ShaderProgram& shader = resolve_shader(marker, shader_cache, shader_spec);
    bool shader_changed = (last_shader != &shader);

    if (shader_changed) {
      last_shader = &shader;
      apply_slot_uniforms_per_view(
        slot_registry, render_slot_ctx, cmd.material, shader
      );
    }

    bool material_changed = shader_changed || last_material == nullptr ||
                            (*last_material != cmd.material);
    if (material_changed) {
      last_material = &cmd.material;

      SlotPrepareContext prepare_ctx{
        .texture_assets = &texture_assets,
        .instanced = true,
      };

      prepare_material(
        marker,
        cmd.material,
        texture_assets,
        shader_cache,
        slot_registry,
        prepare_ctx,
        shader_spec,
        last_prepared
      );

      cmd.material.apply(shader);

      apply_slot_uniforms_per_material(
        slot_registry, render_slot_ctx, cmd.material, last_prepared, shader
      );
    }

    mesh->draw_instanced(
      last_prepared.material,
      cmd.instances.data(),
      static_cast<i32>(cmd.instances.size())
    );
  }

  auto draw_command(RenderCommand const& cmd) -> void {
    draw_command(cmd, cmd.material.shader);
  }

  auto end_pass(RenderConfig const& config = RenderConfig{}) -> void {
    render_state.apply(config);
  }
};

class ClearPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "ClearPass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(K_MAIN_TARGET, {});
  }
  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto const* view = ctx.world.get_resource<ExtractedView>();
    if (view == nullptr) {
      return;
    }
    auto const& target = runtime.get_target(K_MAIN_TARGET);
    target.begin();
    rl::ClearBackground(view->clear_color);
    target.end();
  }
};

class ShadowPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "ShadowPass";
  }
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(RenderPassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* phase = ctx.world.get_resource<ShadowPhase>();
    if ((phase == nullptr) || phase->commands.empty()) {
      return;
    }
    // TODO: DODAĆ SHADOW MAPPING
  }
};

class SkyboxPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "SkyboxPass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(K_MAIN_TARGET);
    builder.write(K_MAIN_TARGET, {});
  }

  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto const* view = ctx.world.get_resource<ExtractedView>();
    if ((view == nullptr) || (view->active_skybox == nullptr) ||
        !view->active_skybox->loaded) {
      return;
    }

    auto const& target = runtime.get_target(K_MAIN_TARGET);
    target.begin();
    ctx.render_state.begin_view(ctx.render_state.view());
    auto const& active_view = ctx.render_state.view();
    view->active_skybox->draw(active_view.view, active_view.projection);
    ctx.render_state.end_view();
    target.end();
    ctx.render_state.reset();
  }
};

class OpaquePass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "OpaquePass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(K_MAIN_TARGET, {});
  }

  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto* state = ctx.world.get_resource<RenderExecutionState>();
    if ((state == nullptr) || !state->world_pass_active) {
      return;
    }

    auto* phase = ctx.world.get_resource<OpaquePhase>();
    auto* mesh_assets = ctx.world.get_resource<Assets<Mesh>>();
    auto* texture_assets = ctx.world.get_resource<Assets<Texture>>();
    auto* shader_cache = ctx.world.get_resource<ShaderCache>();
    auto* slot_registry = ctx.world.get_resource<SlotProviderRegistry>();
    auto* view = ctx.world.get_resource<ExtractedView>();
    auto* lights = ctx.world.get_resource<ExtractedLights>();
    auto* cluster_config = ctx.world.get_resource<ClusterConfig>();
    auto* cluster_state = ctx.world.get_resource<ClusteredLightingState>();

    if ((phase == nullptr) || (nullptr == mesh_assets) ||
        (nullptr == texture_assets) || (shader_cache == nullptr) ||
        (slot_registry == nullptr) || (view == nullptr) ||
        (lights == nullptr) || (cluster_config == nullptr) ||
        (cluster_state == nullptr)) {
      return;
    }

    auto const& target = runtime.get_target(K_MAIN_TARGET);
    target.begin();
    ctx.render_state.begin_view(ctx.render_state.view());

    RenderContext render_ctx{
      ctx.marker,
      ctx.render_state,
      *mesh_assets,
      *texture_assets,
      *shader_cache,
      *slot_registry,
      *view,
      *lights,
      *cluster_config,
      *cluster_state
    };

    render_ctx.begin_pass(RenderConfig::opaque());
    for (auto const& cmd : phase->commands) {
      render_ctx.draw_command(cmd);
    }
    render_ctx.end_pass();
    ctx.render_state.end_view();
    target.end();
  }
};

class TransparentPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "TransparentPass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(K_MAIN_TARGET);
    builder.write(K_MAIN_TARGET, {});
  }

  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto* state = ctx.world.get_resource<RenderExecutionState>();
    if ((state == nullptr) || !state->world_pass_active) {
      return;
    }

    auto* phase = ctx.world.get_resource<TransparentPhase>();
    auto* mesh_assets = ctx.world.get_resource<Assets<Mesh>>();
    auto* texture_assets = ctx.world.get_resource<Assets<Texture>>();
    auto* shader_cache = ctx.world.get_resource<ShaderCache>();
    auto* slot_registry = ctx.world.get_resource<SlotProviderRegistry>();
    auto* view = ctx.world.get_resource<ExtractedView>();
    auto* lights = ctx.world.get_resource<ExtractedLights>();
    auto* cluster_config = ctx.world.get_resource<ClusterConfig>();
    auto* cluster_state = ctx.world.get_resource<ClusteredLightingState>();

    if ((phase == nullptr) || (nullptr == mesh_assets) ||
        (texture_assets == nullptr) || (shader_cache == nullptr) ||
        (slot_registry == nullptr) || (view == nullptr) ||
        (lights == nullptr) || (cluster_config == nullptr) ||
        (cluster_state == nullptr)) {
      return;
    }

    auto const& target = runtime.get_target(K_MAIN_TARGET);
    target.begin();
    ctx.render_state.begin_view(ctx.render_state.view());

    RenderContext render_ctx{
      ctx.marker,
      ctx.render_state,
      *mesh_assets,
      *texture_assets,
      *shader_cache,
      *slot_registry,
      *view,
      *lights,
      *cluster_config,
      *cluster_state
    };

    render_ctx.begin_pass(RenderConfig::transparent());
    for (auto const& cmd : phase->commands) {
      render_ctx.draw_command(cmd);
    }
    render_ctx.end_pass();
    ctx.render_state.end_view();
    target.end();
  }
};

class DepthPrepass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "DepthPrepass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(K_MAIN_TARGET, {});
  }
  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto* state = ctx.world.get_resource<RenderExecutionState>();
    if (!state || !state->world_pass_active) {
      return;
    }

    auto* phase = ctx.world.get_resource<OpaquePhase>();
    auto* mesh_assets = ctx.world.get_resource<Assets<Mesh>>();
    auto* texture_assets = ctx.world.get_resource<Assets<Texture>>();
    auto* shader_cache = ctx.world.get_resource<ShaderCache>();
    auto* slot_registry = ctx.world.get_resource<SlotProviderRegistry>();
    auto* view = ctx.world.get_resource<ExtractedView>();
    auto* lights = ctx.world.get_resource<ExtractedLights>();
    auto* cluster_config = ctx.world.get_resource<ClusterConfig>();
    auto* cluster_state = ctx.world.get_resource<ClusteredLightingState>();

    if (!phase || !mesh_assets || !texture_assets || !shader_cache ||
        !slot_registry || !view || !lights || !cluster_config ||
        !cluster_state) {
      return;
    }

    auto const& target = runtime.get_target(K_MAIN_TARGET);
    target.begin();
    ctx.render_state.begin_view(ctx.render_state.view());

    RenderContext render_ctx{
      ctx.marker,
      ctx.render_state,
      *mesh_assets,
      *texture_assets,
      *shader_cache,
      *slot_registry,
      *view,
      *lights,
      *cluster_config,
      *cluster_state
    };

    render_ctx.begin_pass(RenderConfig::depth_prepass());
    for (auto const& cmd : phase->commands) {
      render_ctx.draw_command(cmd, depth_prepass_shader_spec());
    }
    render_ctx.end_pass(RenderConfig::post_depth_prepass());
    ctx.render_state.end_view();
    target.end();
  }

private:
  static auto depth_prepass_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return SPEC;
  }
};

class ClusterBuildPass : public RenderPass {
public:
  auto name() const -> std::string override { return "ClusterBuildPass"; }
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(RenderPassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* state = ctx.world.get_resource<RenderExecutionState>();
    if (!state || !state->world_pass_active) {
      return;
    }

    auto* view = ctx.world.get_resource<ExtractedView>();
    auto* lights = ctx.world.get_resource<ExtractedLights>();
    auto* cluster_config = ctx.world.get_resource<ClusterConfig>();
    auto* cluster_state = ctx.world.get_resource<ClusteredLightingState>();

    if (!view || !lights || !cluster_config || !cluster_state) {
      return;
    }

    cluster_state->build_cluster_buffers(
      ctx.marker, *view, *lights, *cluster_config
    );
  }
};

struct WorldPass {
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

  static auto begin(
    NonSendMarker marker,
    gl::RenderState& render_state,
    ExtractedView const& view
  ) -> void {
    (void) marker;

    render_state.begin_view(
      gl::RenderViewState{
        .viewport = view.viewport,
        .view = view.camera_view.view,
        .projection = view.camera_view.proj,
        .camera_position = view.camera_view.position,
        .orthographic = view.orthographic,
        .near_plane = view.near_plane,
        .far_plane = view.far_plane,
      }
    );

    render_state.apply(RenderConfig::get_default());
  }

  static auto end(NonSendMarker marker, gl::RenderState& render_state) -> void {
    (void) marker;
    render_state.end_view();
    render_state.apply(RenderConfig{});
  }
};

struct FramePass {
  static auto begin(
    Res<ExtractedView> view,
    ResMut<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    NonSendMarker marker
  ) -> void {
    render_state->reset();
    state->world_pass_active = false;
    state->world_target_active = false;
    state->backbuffer_active = false;
    state->scene_active = view->active;

    if (!view->active) {
      state->begin();
      state->clear(view->clear_color);
      state->backbuffer_active = true;
      return;
    }

    // The RenderGraph will handle its own targets.
    // However, we still need to call WorldPass::begin to set the view matrix.
    WorldPass::begin(marker, *render_state, *view);
    state->world_pass_active = true;
  }

  static auto execute_graph(
    ResMut<RenderGraph> graph,
    ResMut<gl::RenderState> render_state,
    World& world,
    NonSendMarker marker
  ) -> void {
    graph->execute(world, *render_state, marker);
  }

  static auto end(
    ResMut<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    Res<DebugOverlaySettings> debug_settings,
    NonSendMarker marker
  ) -> void {
    if (state->world_pass_active) {
      WorldPass::end(marker, *render_state);
      state->world_pass_active = false;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->backbuffer_active = true;
    }

    if (debug_settings->draw_fps) {
      gl::draw_fps(
        static_cast<i32>(debug_settings->fps_position.x),
        static_cast<i32>(debug_settings->fps_position.y)
      );
    }

    if (state->backbuffer_active) {
      state->end();
      state->backbuffer_active = false;
    }
  }
};

class PostProcessPassNode : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override {
    return "PostProcessPass";
  }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(K_MAIN_TARGET);
    builder.write_backbuffer("Screen");
  }
  auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void override {
    auto* state = ctx.world.get_resource<RenderExecutionState>();
    auto* shader_cache = ctx.world.get_resource<ShaderCache>();
    auto* view = ctx.world.get_resource<ExtractedView>();
    auto* time = ctx.world.get_resource<Time>();

    if (!state || !shader_cache || !view || !time) {
      return;
    }

    if (state->world_pass_active) {
      WorldPass::end(ctx.marker, ctx.render_state);
      state->world_pass_active = false;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->clear(Color::BLACK);
      state->backbuffer_active = true;
    }

    if (!view->active) {
      return;
    }

    auto const& main_color = runtime.get_texture(K_MAIN_TARGET);
    if (!main_color.ready()) {
      return;
    }

    draw_postprocess_pass(ctx.marker, main_color, *shader_cache, *view, *time);
    ctx.render_state.reset();
  }

private:
  static auto draw_world_target_to_screen(
    gl::Texture const& source_texture, ExtractedView const& view
  ) -> void {
    gl::draw_texture_pro(
      source_texture,
      source_texture.rect().flipped_y(),
      view.viewport,
      {0.0F, 0.0F},
      0.0F,
      Color::WHITE
    );
  }

  static auto apply_postprocess_uniforms(
    ShaderProgram& shader,
    PostProcessDescriptor const& effect,
    ExtractedView const& view,
    Time const& time
  ) -> void {
    Vec2 const resolution = {
      std::max(view.viewport.width, 1.0F),
      std::max(view.viewport.height, 1.0F),
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
      draw_world_target_to_screen(source_texture, view);
      return;
    }

    ShaderProgram& shader =
      resolve_shader(marker, shader_cache, view.postprocess.shader);
    apply_postprocess_uniforms(shader, view.postprocess, view, time);
    shader.set_texture(MaterialMap::Albedo, source_texture);

    shader.with_bound([&] -> void {
      draw_world_target_to_screen(source_texture, view);
    });
  }
};

struct PostProcessPass {
  static auto postprocess(
    ResMut<RenderExecutionState> state,
    ResMut<gl::RenderState> render_state,
    ResMut<RenderGraph> graph,
    ResMut<ShaderCache> shader_cache,
    Res<ExtractedView> view,
    Res<Time> time,
    NonSendMarker marker
  ) -> void {
    // This system is now redundant if the graph contains PostProcessPassNode
    // But we keep it for now if we want to run it as a separate system.
  }
};

class UiPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override { return "UiPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(K_MAIN_TARGET); // UI draws on top of everything
    builder.write_backbuffer("Screen");
  }
  auto execute(RenderPassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* phase = ctx.world.get_resource<UiPhase>();
    auto* texture_assets = ctx.world.get_resource<Assets<Texture>>();
    auto* state = ctx.world.get_resource<RenderExecutionState>();

    if (!phase || !texture_assets || !state) {
      return;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->backbuffer_active = true;
    }

    ctx.render_state.apply(RenderConfig{});

    for (auto const& item : phase->items) {
      auto const* texture = texture_assets->get(item.texture);
      if (texture == nullptr) {
        continue;
      }

      Vec2 position = {
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
      Vec2 origin = {0, 0};

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
    return std::atan2(siny_cosp, cosy_cosp) *
           (180.0F / std::numbers::pi_v<float>);
  }
};

} // namespace ecs
