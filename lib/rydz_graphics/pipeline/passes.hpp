#pragma once

#include "rl.hpp"
#include "rydz_ecs/core/time.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/gl/state.hpp"
#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/pipeline/config.hpp"
#include "rydz_graphics/pipeline/graph.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace ecs {

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

struct EnvironmentRenderer {
  using T = Resource;
  gl::Skybox skybox{};

  static auto initialize_environment_renderer(
    ResMut<EnvironmentRenderer> renderer, NonSendMarker
  ) -> void {
    if (renderer->skybox.ready()) {
      return;
    }

    renderer->skybox = gl::Skybox::create(
      gl::ShaderProgram::load(
        gl::ShaderSpec::from("res/shaders/skybox.vert", "res/shaders/skybox.frag")
      )
    );
  }
};

inline auto main_render_view(ExtractedView const& view) -> gl::RenderViewState {
  return gl::RenderViewState{
    .viewport = view.viewport,
    .view = view.camera_view.view,
    .projection = view.camera_view.proj,
    .camera_position = view.camera_view.position,
    .orthographic = view.orthographic,
    .near_plane = view.near_plane,
    .far_plane = view.far_plane,
  };
}

inline auto shadow_render_view(ShadowView const& view) -> gl::RenderViewState {
  return gl::RenderViewState{
    .viewport = {0.0F, 0.0F, 1.0F, 1.0F},
    .view = view.view,
    .projection = view.projection,
    .camera_position = view.position,
    .orthographic = view.orthographic,
    .near_plane = view.near_plane,
    .far_plane = view.far_plane,
  };
}

class PassRenderer {
public:
  explicit PassRenderer(
    PassContext& ctx, RenderConfig const& config = {}, bool instanced = true
  )
      : ctx_(ctx), material_ctx_{.frame_data = &ctx, .instanced = instanced},
        pass_config_(config) {
    last_prepared_ = {};
    ctx_.render_state.apply(pass_config_);
  }
  auto end(RenderConfig const& config = RenderConfig{}) -> void {
    ctx_.render_state.apply(config);
  }

  auto begin(RenderConfig const& config) -> void {}

  auto draw(RenderCommand const& cmd, ShaderSpec const& shader_spec) -> void {
    auto const* mesh = ctx_.mesh_assets.get(cmd.mesh);
    if (mesh == nullptr) {
      return;
    }
    if (cmd.material_index >= ctx_.extracted_meshes.materials.size()) {
      return;
    }
    auto const& material_item = ctx_.extracted_meshes.materials[cmd.material_index];
    auto const& material = material_item.material;

    pass_config_.cull = material.cull_state();
    ctx_.render_state.apply(pass_config_);

    ShaderProgram& shader = resolve_shader(ctx_.marker, ctx_.shader_cache, shader_spec);
    bool const shader_changed = (last_shader_ != &shader);

    if (shader_changed) {
      last_shader_ = &shader;
      ctx_.shadow_uniforms.bind_shader(shader);
      ctx_.shadow_resources.bind_shader(shader);
      if (ctx_.scene_depth_texture != nullptr && ctx_.scene_depth_texture->ready()) {
        ctx_.scene_depth_texture->bind(SCENE_DEPTH_TEXTURE_SLOT);
        shader.set_sampler("u_scene_depth", SCENE_DEPTH_TEXTURE_SLOT);
      }
      apply_slot_uniforms_per_view(material_ctx_, material, shader);
    }

    bool const material_changed =
      shader_changed || last_material_ == nullptr || (*last_material_ != material);
    if (material_changed) {
      last_material_ = &material;

      prepare_material(material_ctx_, material, shader_spec, last_prepared_);
      material.apply(shader);
      apply_slot_uniforms_per_material(material_ctx_, material, last_prepared_, shader);
    }

    mesh->draw_instanced(
      last_prepared_.material,
      cmd.instances.data(),
      static_cast<i32>(cmd.instances.size())
    );
  }

  auto draw(RenderCommand const& cmd) -> void {
    if (cmd.material_index >= ctx_.extracted_meshes.materials.size()) {
      return;
    }
    draw(cmd, ctx_.extracted_meshes.materials[cmd.material_index].material.shader);
  }

private:
  PassContext& ctx_;
  MaterialContext material_ctx_;
  RenderConfig pass_config_{};
  ShaderProgram* last_shader_ = nullptr;
  CompiledMaterial const* last_material_ = nullptr;
  PreparedMaterial last_prepared_{};
};

class ClearPass : public RenderPass {
public:
  explicit ClearPass(RenderTextureHandle main_target) : main_target_(main_target) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(main_target_);
  }
  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override {
    rl::ClearBackground(ctx.view.clear_color);
  }

private:
  RenderTextureHandle main_target_;
};

class ShadowPass : public RenderPass {
public:
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(PassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* state = ctx.execution_state;
    if ((state == nullptr) || !state->world_pass_active) {
      return;
    }
    if (ctx.shadow_phase.commands.empty()) {
      return;
    }

    ctx.shadow_resources.ensure(ctx.shadow_settings);

    if (ctx.shadows.has_directional) {
      ctx.shadow_resources.allocate_cascades(
        const_cast<ExtractedShadows&>(ctx.shadows), ctx.shadow_settings, ctx.view
      );
    }
    
    if (!ctx.shadows.point_shadows.empty()) {
      ctx.shadow_resources.allocate_point_lights(
        const_cast<ExtractedShadows&>(ctx.shadows),
        ctx.shadow_settings,
        ctx.view,
        ctx.lights
      );
    }

    if (ctx.shadows.cascade_count > 0 && ctx.shadow_resources.directional_atlas.ready()) {
      ctx.shadow_resources.directional_atlas.begin();
      for (i32 cascade_index = 0; cascade_index < ctx.shadows.cascade_count;
           ++cascade_index) {
        auto const tile = ctx.shadow_resources.directional_tile(cascade_index);
        ctx.shadow_resources.directional_atlas.begin_tile(tile);
        render_shadow_commands(
          ctx,
          ctx.shadows.directional_cascades[cascade_index].shadow_view,
          directional_shadow_shader_spec()
        );
      }
      ctx.shadow_resources.directional_atlas.end();
    }

    for (usize point_index = 0; point_index < ctx.shadows.point_shadows.size();
         ++point_index) {
      auto const& point_shadow = ctx.shadows.point_shadows[point_index];

      if (!ctx.shadow_resources.point_maps.ready()) {
        break;
      }

      for (i32 face_index = 0; face_index < POINT_SHADOW_FACE_COUNT; ++face_index) {
        ctx.shadow_resources.point_maps.begin_face(
          static_cast<i32>(point_index), face_index
        );
        render_point_shadow_commands(
          ctx,
          point_shadow,
          face_index,
          point_shadow_shader_spec()
        );
        ctx.shadow_resources.point_maps.end();
      }
    }

    ctx.render_state.reset();
    ctx.render_state.begin_view(main_render_view(ctx.view));
    ctx.render_state.apply(RenderConfig::get_default());
  }

private:
  static auto directional_shadow_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return SPEC;
  }

  static auto point_shadow_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/point_shadow.vert", "res/shaders/point_shadow.frag");
    return SPEC;
  }

  static auto shadow_frustum_planes(ShadowView const& shadow_view)
    -> std::array<FrustumPlane, 6> {
    return extract_frustum_planes(shadow_view.view_projection);
  }

  static auto shadow_command_visible(
    RenderCommand const& cmd, std::array<FrustumPlane, 6> const& planes
  ) -> bool {
    return aabb_in_frustum(cmd.world_bounds, planes);
  }

  static auto render_shadow_commands(
    PassContext& ctx, ShadowView const& shadow_view, ShaderSpec const& shader_spec
  ) -> void {
    auto const planes = shadow_frustum_planes(shadow_view);
    ctx.render_state.reset();
    ctx.render_state.begin_view(shadow_render_view(shadow_view));

    PassRenderer renderer{ctx, RenderConfig::depth_prepass()};
    for (auto const& cmd : ctx.shadow_phase.commands) {
      if (!shadow_command_visible(cmd, planes)) {
        continue;
      }
      renderer.draw(cmd, shader_spec);
    }
    renderer.end(RenderConfig::post_depth_prepass());
    ctx.render_state.end_view();
    ctx.render_state.reset();
  }

  static auto render_point_shadow_commands(
    PassContext& ctx,
    ExtractedShadows::PointShadow const& point_shadow,
    i32 face_index,
    ShaderSpec const& shader_spec
  ) -> void {
    auto const& shadow_view = point_shadow.faces[static_cast<usize>(face_index)];
    auto const planes = shadow_frustum_planes(shadow_view);
    ctx.render_state.reset();
    ctx.render_state.begin_view(shadow_render_view(shadow_view));

    PassRenderer renderer{ctx, RenderConfig::depth_prepass()};
    for (auto const& cmd : ctx.shadow_phase.commands) {
      if (!shadow_command_visible(cmd, planes)) {
        continue;
      }
      ShaderProgram& shader = resolve_shader(ctx.marker, ctx.shader_cache, shader_spec);
      shader.set("u_light_pos", point_shadow.position);
      shader.set("u_far_plane", point_shadow.far_plane);
      renderer.draw(cmd, shader_spec);
    }
    renderer.end(RenderConfig::post_depth_prepass());
    ctx.render_state.end_view();
    ctx.render_state.reset();
  }
};

class EnvironmentPass : public RenderPass {
public:
  explicit EnvironmentPass(RenderTextureHandle main_target) : main_target_(main_target) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(main_target_);
  }

  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override {
    auto const* renderer = ctx.environment_renderer;
    if ((renderer == nullptr) || !renderer->skybox.ready() ||
        (ctx.view.active_environment == nullptr) ||
        !ctx.view.active_environment->has_skybox()) {
      return;
    }

    ctx.render_state.begin_view(ctx.render_state.view());
    auto const& active_view = ctx.render_state.view();
    renderer->skybox.draw(
      ctx.view.active_environment->skybox, active_view.view, active_view.projection
    );
    ctx.render_state.end_view();
    ctx.render_state.reset();
  }

private:
  RenderTextureHandle main_target_;
};

template <typename Tag> class MeshPass : public RenderPass {
public:
  MeshPass(RenderTextureHandle main_target, RenderTextureHandle scene_depth)
      : main_target_(main_target), scene_depth_(scene_depth) {}

  auto setup(RenderGraphBuilder& builder) -> void override;
  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override;

private:
  RenderTextureHandle main_target_;
  RenderTextureHandle scene_depth_;

  static auto render_config() -> RenderConfig;
};

template <> inline auto MeshPass<OpaqueTag>::setup(RenderGraphBuilder& builder) -> void {
  builder.read(scene_depth_);
  builder.write(main_target_);
}

template <> inline auto MeshPass<OpaqueTag>::render_config() -> RenderConfig {
  return RenderConfig::opaque();
}

template <>
inline auto MeshPass<OpaqueTag>::execute(PassContext& ctx, RenderGraphRuntime& runtime)
  -> void {
  auto* state = ctx.execution_state;
  if ((state == nullptr) || !state->world_pass_active) {
    return;
  }
  if (ctx.opaque_phase.commands.empty()) {
    return;
  }
  auto const& scene_depth_target = runtime.get_target(scene_depth_);
  ctx.scene_depth_texture =
    scene_depth_target.depth.ready() ? &scene_depth_target.depth : nullptr;
  ctx.render_state.begin_view(ctx.render_state.view());
  ctx.view_uniforms.bind();
  PassRenderer renderer{ctx, MeshPass<OpaqueTag>::render_config()};
  for (auto const& cmd : ctx.opaque_phase.commands) {
    renderer.draw(cmd);
  }
  renderer.end();
  ctx.render_state.end_view();
  ctx.scene_depth_texture = nullptr;
}

template <>
inline auto MeshPass<TransparentTag>::setup(RenderGraphBuilder& builder) -> void {
  builder.read(scene_depth_);
  builder.read(main_target_);
  builder.write(main_target_);
}

template <> inline auto MeshPass<TransparentTag>::render_config() -> RenderConfig {
  return RenderConfig{};
}

template <>
inline auto MeshPass<TransparentTag>::execute(
  PassContext& ctx, RenderGraphRuntime& runtime
) -> void {
  auto* state = ctx.execution_state;
  if ((state == nullptr) || !state->world_pass_active) {
    return;
  }
  if (ctx.transparent_phase.commands.empty()) {
    return;
  }
  auto const& scene_depth_target = runtime.get_target(scene_depth_);
  ctx.scene_depth_texture =
    scene_depth_target.depth.ready() ? &scene_depth_target.depth : nullptr;
  ctx.render_state.begin_view(ctx.render_state.view());
  ctx.view_uniforms.bind();
  PassRenderer renderer{ctx, MeshPass<TransparentTag>::render_config()};
  for (auto const& cmd : ctx.transparent_phase.commands) {
    renderer.draw(cmd);
  }
  renderer.end();
  ctx.render_state.end_view();
  ctx.scene_depth_texture = nullptr;
}

using OpaquePass = MeshPass<OpaqueTag>;
using TransparentPass = MeshPass<TransparentTag>;

class DepthPrepass : public RenderPass {
public:
  explicit DepthPrepass(RenderTextureHandle main_target) : main_target_(main_target) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(main_target_);
  }
  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override {
    auto* state = ctx.execution_state;
    if (!state || !state->world_pass_active) {
      return;
    }

    if (ctx.opaque_phase.commands.empty()) {
      return;
    }

    ctx.render_state.begin_view(ctx.render_state.view());

    ctx.view_uniforms.bind();
    PassRenderer renderer{ctx};
    for (auto const& cmd : ctx.opaque_phase.commands) {
      renderer.draw(cmd, depth_prepass_shader_spec());
    }
    renderer.end(RenderConfig::post_depth_prepass());
    ctx.render_state.end_view();
  }

private:
  static auto depth_prepass_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return SPEC;
  }

  RenderTextureHandle main_target_;
};

class DepthCopyPass : public RenderPass {
public:
  DepthCopyPass(RenderTextureHandle source, RenderTextureHandle destination)
      : source_(source), destination_(destination) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(source_);
    builder.write(destination_);
  }

  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override {
    auto* state = ctx.execution_state;
    if (!state || !state->world_pass_active) {
      return;
    }

    auto const& source = runtime.get_target(source_);
    auto const& destination = runtime.get_target(destination_);
    if (!source.ready() || !destination.ready() || source.depth.id == 0 ||
        destination.depth.id == 0) {
      return;
    }

    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination.id);
    glBlitFramebuffer(
      0,
      0,
      source.texture.width,
      source.texture.height,
      0,
      0,
      destination.texture.width,
      destination.texture.height,
      GL_DEPTH_BUFFER_BIT,
      GL_NEAREST
    );
    glBindFramebuffer(GL_FRAMEBUFFER, destination.id);
  }

private:
  RenderTextureHandle source_;
  RenderTextureHandle destination_;
};

class ClusterBuildPass : public RenderPass {
public:
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(PassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* state = ctx.execution_state;
    if (!state || !state->world_pass_active) {
      return;
    }

    ctx.cluster_state.build_cluster_buffers(
      ctx.marker, ctx.view, ctx.lights, ctx.shadows, ctx.cluster_config
    );
  }
};

struct WorldPass {
  static auto cluster_build(
    Res<RenderExecutionState> state,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ExtractedShadows> shadows,
    Res<ClusterConfig> cluster_config,
    ResMut<ClusteredLightingState> cluster_state,
    NonSendMarker marker
  ) -> void {
    if (!state->world_pass_active) {
      return;
    }

    cluster_state->build_cluster_buffers(
      marker, *view, *lights, *shadows, *cluster_config
    );
  }

  static auto begin(
    NonSendMarker marker, gl::RenderState& render_state, ExtractedView const& view
  ) -> void {
    (void) marker;

    render_state.begin_view(main_render_view(view));

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

    WorldPass::begin(marker, *render_state, *view);
    state->world_pass_active = true;
  }

  static auto execute_graph(
    ResMut<RenderGraph> graph,
    ResMut<gl::RenderState> render_state,
    ResMut<RenderExecutionState> state,
    Res<EnvironmentRenderer> environment_renderer,
    Res<Window> window,
    Res<ShadowPhase> shadow_phase,
    Res<OpaquePhase> opaque_phase,
    Res<TransparentPhase> transparent_phase,
    Res<UiPhase> ui_phase,
    Res<ExtractedMeshes> extracted_meshes,
    Res<Assets<Mesh>> mesh_assets,
    Res<Assets<Texture>> texture_assets,
    ResMut<ShaderCache> shader_cache,
    ResMut<ViewUniformState> view_uniforms,
    ResMut<ShadowUniformState> shadow_uniforms,
    Res<SlotProviderRegistry> slot_registry,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ExtractedShadows> shadows,
    Res<ClusterConfig> cluster_config,
    Res<ShadowSettings> shadow_settings,
    ResMut<ShadowResources> shadow_resources,
    ResMut<ClusteredLightingState> cluster_state,
    Res<Time> time,
    NonSendMarker marker
  ) -> void {
    view_uniforms->update(*view, *lights, *cluster_config);
    shadow_resources->ensure(*shadow_settings);

    if (shadows->has_directional) {
      shadow_resources->allocate_cascades(
        const_cast<ExtractedShadows&>(*shadows), *shadow_settings, *view
      );
    }
    
    if (!shadows->point_shadows.empty()) {
      shadow_resources->allocate_point_lights(
        const_cast<ExtractedShadows&>(*shadows),
        *shadow_settings,
        *view,
        *lights
      );
    }

    shadow_uniforms->update(*shadows, *shadow_settings, *shadow_resources);

    PassContext ctx{
      .marker = marker,
      .render_state = *render_state,
      .framebuffer =
        gl::Rectangle{
          0, 0, static_cast<f32>(window->width), static_cast<f32>(window->height)
        },
      .view = *view,
      .lights = *lights,
      .shadows = *shadows,
      .time = *time,
      .extracted_meshes = *extracted_meshes,
      .mesh_assets = *mesh_assets,
      .texture_assets = *texture_assets,
      .shader_cache = *shader_cache,
      .view_uniforms = *view_uniforms,
      .shadow_uniforms = *shadow_uniforms,
      .slot_registry = *slot_registry,
      .opaque_phase = *opaque_phase,
      .transparent_phase = *transparent_phase,
      .shadow_phase = *shadow_phase,
      .ui_phase = *ui_phase,
      .cluster_config = *cluster_config,
      .shadow_settings = *shadow_settings,
      .cluster_state = *cluster_state,
      .shadow_resources = *shadow_resources,
      .scene_depth_texture = nullptr,
      .execution_state = state.ptr,
      .environment_renderer = environment_renderer.ptr,
    };
    graph->execute(ctx);
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
  PostProcessPassNode(RenderTextureHandle main_target, RenderTextureHandle screen)
      : main_target_(main_target), screen_(screen) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(screen_);
  }
  auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void override {
    auto* state = ctx.execution_state;

    if (!state) {
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

    if (!ctx.view.active) {
      return;
    }

    auto const& main_color = runtime.get_texture(main_target_);
    if (!main_color.ready()) {
      return;
    }

    draw_postprocess_pass(ctx.marker, main_color, ctx.shader_cache, ctx.view, ctx.time);
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

    ShaderProgram& shader = resolve_shader(marker, shader_cache, view.postprocess.shader);
    apply_postprocess_uniforms(shader, view.postprocess, view, time);
    shader.set_texture(MaterialMap::Albedo, source_texture);

    shader.with_bound([&] -> void { draw_world_target_to_screen(source_texture, view); });
  }

  RenderTextureHandle main_target_;
  RenderTextureHandle screen_;
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
  ) -> void {}
};

class UiPass : public RenderPass {
public:
  UiPass(RenderTextureHandle main_target, RenderTextureHandle screen)
      : main_target_(main_target), screen_(screen) {}

  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(screen_);
  }
  auto execute(PassContext& ctx, RenderGraphRuntime&) -> void override {
    auto* state = ctx.execution_state;

    if (!state) {
      return;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->backbuffer_active = true;
    }

    ctx.render_state.apply(RenderConfig{});

    for (auto const& item : ctx.ui_phase.items) {
      auto const* texture = ctx.texture_assets.get(item.texture);
      if (texture == nullptr) {
        continue;
      }

      Vec2 position = {item.transform.translation.x, item.transform.translation.y};
      gl::Rectangle source = {
        0, 0, static_cast<f32>(texture->width), static_cast<f32>(texture->height)
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
    f32 cosy_cosp = 1.0F - (2.0F * (transform.rotation.y * transform.rotation.y +
                                    transform.rotation.z * transform.rotation.z));
    return std::atan2(siny_cosp, cosy_cosp) * (180.0F / std::numbers::pi_v<float>);
  }

  RenderTextureHandle main_target_;
  RenderTextureHandle screen_;
};

} // namespace ecs
