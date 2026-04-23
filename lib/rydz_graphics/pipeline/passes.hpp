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
};

inline auto initialize_environment_renderer(
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

class PassRenderer {
public:
  explicit PassRenderer(
    FrameResources& frame, RenderConfig const& config = {}, bool instanced = true
  )
      : frame_(frame), material_ctx_{.frame_data = &frame, .instanced = instanced},
        pass_config_(config) {
    last_prepared_ = {};
    frame_.render_state.apply(pass_config_);
  }
  auto end(RenderConfig const& config = RenderConfig{}) -> void {
    frame_.render_state.apply(config);
  }

  static auto is_ready(FrameResources const& frame) -> bool {
    return frame.mesh_assets != nullptr && frame.texture_assets != nullptr &&
           frame.shader_cache != nullptr && frame.slot_registry != nullptr &&
           frame.view != nullptr && frame.lights != nullptr &&
           frame.cluster_config != nullptr && frame.cluster_state != nullptr &&
           frame.time != nullptr;
  }

  auto begin(RenderConfig const& config) -> void {}

  auto draw(RenderCommand const& cmd, ShaderSpec const& shader_spec) -> void {
    auto const* mesh = frame_.mesh_assets->get(cmd.mesh);
    if (mesh == nullptr) {
      return;
    }

    pass_config_.cull = cmd.material.cull_state();
    frame_.render_state.apply(pass_config_);

    ShaderProgram& shader =
      resolve_shader(frame_.marker, *frame_.shader_cache, shader_spec);
    bool const shader_changed = (last_shader_ != &shader);

    if (shader_changed) {
      last_shader_ = &shader;
      apply_slot_uniforms_per_view(material_ctx_, cmd.material, shader);
    }

    bool const material_changed =
      shader_changed || last_material_ == nullptr || (*last_material_ != cmd.material);
    if (material_changed) {
      last_material_ = &cmd.material;

      prepare_material(material_ctx_, cmd.material, shader_spec, last_prepared_);
      cmd.material.apply(shader);
      apply_slot_uniforms_per_material(
        material_ctx_, cmd.material, last_prepared_, shader
      );
    }

    mesh->draw_instanced(
      last_prepared_.material,
      cmd.instances.data(),
      static_cast<i32>(cmd.instances.size())
    );
  }

  auto draw(RenderCommand const& cmd) -> void { draw(cmd, cmd.material.shader); }

private:
  FrameResources& frame_;
  MaterialContext material_ctx_;
  RenderConfig pass_config_{};
  ShaderProgram* last_shader_ = nullptr;
  CompiledMaterial const* last_material_ = nullptr;
  PreparedMaterial last_prepared_{};
};

class ClearPass : public RenderPass {
public:
  explicit ClearPass(RenderTextureHandle main_target) : main_target_(main_target) {}

  [[nodiscard]] auto name() const -> std::string override { return "ClearPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(main_target_);
  }
  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto const* view = frame.view;
    if (view == nullptr) {
      return;
    }
    auto const& target = runtime.get_target(main_target_);
    target.begin();
    rl::ClearBackground(view->clear_color);
    target.end();
  }

private:
  RenderTextureHandle main_target_;
};

class ShadowPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override { return "ShadowPass"; }
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(FrameResources& frame, RenderGraphRuntime&) -> void override {
    auto const* phase = frame.shadow_phase;
    if ((phase == nullptr) || phase->commands.empty()) {
      return;
    }
    // TODO: DODAĆ SHADOW MAPPING
  }
};

class EnvironmentPass : public RenderPass {
public:
  explicit EnvironmentPass(RenderTextureHandle main_target) : main_target_(main_target) {}

  [[nodiscard]] auto name() const -> std::string override { return "EnvironmentPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(main_target_);
  }

  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto const* view = frame.view;
    auto const* renderer = frame.environment_renderer;
    if ((view == nullptr) || (renderer == nullptr) || !renderer->skybox.ready() ||
        (view->active_environment == nullptr) ||
        !view->active_environment->has_skybox()) {
      return;
    }

    auto const& target = runtime.get_target(main_target_);
    target.begin();
    frame.render_state.begin_view(frame.render_state.view());
    auto const& active_view = frame.render_state.view();
    renderer->skybox.draw(
      view->active_environment->skybox, active_view.view, active_view.projection
    );
    frame.render_state.end_view();
    target.end();
    frame.render_state.reset();
  }

private:
  RenderTextureHandle main_target_;
};

class OpaquePass : public RenderPass {
public:
  explicit OpaquePass(RenderTextureHandle main_target) : main_target_(main_target) {}

  [[nodiscard]] auto name() const -> std::string override { return "OpaquePass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(main_target_);
  }

  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto* state = frame.execution_state;
    if ((state == nullptr) || !state->world_pass_active) {
      return;
    }

    auto const* phase = frame.opaque_phase;
    if ((phase == nullptr) || !PassRenderer::is_ready(frame)) {
      return;
    }

    auto const& target = runtime.get_target(main_target_);
    target.begin();
    frame.render_state.begin_view(frame.render_state.view());

    PassRenderer renderer{frame, RenderConfig::opaque()};
    for (auto const& cmd : phase->commands) {
      renderer.draw(cmd);
    }
    renderer.end();
    frame.render_state.end_view();
    target.end();
  }

private:
  RenderTextureHandle main_target_;
};

class TransparentPass : public RenderPass {
public:
  explicit TransparentPass(RenderTextureHandle main_target) : main_target_(main_target) {}

  [[nodiscard]] auto name() const -> std::string override { return "TransparentPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(main_target_);
  }

  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto* state = frame.execution_state;
    if ((state == nullptr) || !state->world_pass_active) {
      return;
    }

    auto const* phase = frame.transparent_phase;
    if ((phase == nullptr) || !PassRenderer::is_ready(frame)) {
      return;
    }

    auto const& target = runtime.get_target(main_target_);
    target.begin();
    frame.render_state.begin_view(frame.render_state.view());

    PassRenderer renderer{frame};
    for (auto const& cmd : phase->commands) {
      renderer.draw(cmd);
    }
    renderer.end();
    frame.render_state.end_view();
    target.end();
  }

private:
  RenderTextureHandle main_target_;
};

class DepthPrepass : public RenderPass {
public:
  explicit DepthPrepass(RenderTextureHandle main_target) : main_target_(main_target) {}

  [[nodiscard]] auto name() const -> std::string override { return "DepthPrepass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.write(main_target_);
  }
  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto* state = frame.execution_state;
    if (!state || !state->world_pass_active) {
      return;
    }

    auto const* phase = frame.opaque_phase;
    if (!phase || !PassRenderer::is_ready(frame)) {
      return;
    }

    auto const& target = runtime.get_target(main_target_);
    target.begin();
    frame.render_state.begin_view(frame.render_state.view());

    PassRenderer renderer{frame};
    for (auto const& cmd : phase->commands) {
      renderer.draw(cmd, depth_prepass_shader_spec());
    }
    renderer.end(RenderConfig::post_depth_prepass());
    frame.render_state.end_view();
    target.end();
  }

private:
  static auto depth_prepass_shader_spec() -> ShaderSpec const& {
    static ShaderSpec const SPEC =
      ShaderSpec::from("res/shaders/depth.vert", "res/shaders/depth.frag");
    return SPEC;
  }

  RenderTextureHandle main_target_;
};

class ClusterBuildPass : public RenderPass {
public:
  [[nodiscard]] auto name() const -> std::string override { return "ClusterBuildPass"; }
  auto setup(RenderGraphBuilder&) -> void override {}
  auto execute(FrameResources& frame, RenderGraphRuntime&) -> void override {
    auto* state = frame.execution_state;
    if (!state || !state->world_pass_active) {
      return;
    }

    auto const* view = frame.view;
    auto const* lights = frame.lights;
    auto const* cluster_config = frame.cluster_config;
    auto* cluster_state = frame.cluster_state;

    if (!view || !lights || !cluster_config || !cluster_state) {
      return;
    }

    cluster_state->build_cluster_buffers(frame.marker, *view, *lights, *cluster_config);
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

    cluster_state->build_cluster_buffers(marker, *view, *lights, *cluster_config);
  }

  static auto begin(
    NonSendMarker marker, gl::RenderState& render_state, ExtractedView const& view
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
    Res<Assets<Mesh>> mesh_assets,
    Res<Assets<Texture>> texture_assets,
    ResMut<ShaderCache> shader_cache,
    Res<SlotProviderRegistry> slot_registry,
    Res<ExtractedView> view,
    Res<ExtractedLights> lights,
    Res<ClusterConfig> cluster_config,
    ResMut<ClusteredLightingState> cluster_state,
    Res<Time> time,
    NonSendMarker marker
  ) -> void {
    FrameResources frame{
      .marker = marker,
      .render_state = *render_state,
      .framebuffer_width = static_cast<u32>(window->width),
      .framebuffer_height = static_cast<u32>(window->height),
      .execution_state = state.ptr,
      .shadow_phase = shadow_phase.ptr,
      .opaque_phase = opaque_phase.ptr,
      .transparent_phase = transparent_phase.ptr,
      .ui_phase = ui_phase.ptr,
      .mesh_assets = mesh_assets.ptr,
      .texture_assets = texture_assets.ptr,
      .shader_cache = shader_cache.ptr,
      .slot_registry = slot_registry.ptr,
      .environment_renderer = environment_renderer.ptr,
      .view = view.ptr,
      .lights = lights.ptr,
      .cluster_config = cluster_config.ptr,
      .cluster_state = cluster_state.ptr,
      .time = time.ptr,
    };
    graph->execute(frame);
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

  [[nodiscard]] auto name() const -> std::string override { return "PostProcessPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(screen_);
  }
  auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void override {
    auto* state = frame.execution_state;
    auto* shader_cache = frame.shader_cache;
    auto const* view = frame.view;
    auto const* time = frame.time;

    if (!state || !shader_cache || !view || !time) {
      return;
    }

    if (state->world_pass_active) {
      WorldPass::end(frame.marker, frame.render_state);
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

    auto const& main_color = runtime.get_texture(main_target_);
    if (!main_color.ready()) {
      return;
    }

    draw_postprocess_pass(frame.marker, main_color, *shader_cache, *view, *time);
    frame.render_state.reset();
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

  [[nodiscard]] auto name() const -> std::string override { return "UiPass"; }
  auto setup(RenderGraphBuilder& builder) -> void override {
    builder.read(main_target_);
    builder.write(screen_);
  }
  auto execute(FrameResources& frame, RenderGraphRuntime&) -> void override {
    auto const* phase = frame.ui_phase;
    auto const* texture_assets = frame.texture_assets;
    auto* state = frame.execution_state;

    if (!phase || !texture_assets || !state) {
      return;
    }

    if (!state->backbuffer_active) {
      state->begin();
      state->backbuffer_active = true;
    }

    frame.render_state.apply(RenderConfig{});

    for (auto const& item : phase->items) {
      auto const* texture = texture_assets->get(item.texture);
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
