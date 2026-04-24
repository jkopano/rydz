#include <gtest/gtest.h>

#include "rydz_graphics/pipeline/graph.hpp"
#include "rydz_graphics/pipeline/pass_context.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include <string>
#include <utility>
#include <vector>

using namespace ecs;

namespace {

class RecordingPass final : public RenderPass {
public:
  RecordingPass(
    std::string name,
    std::vector<RenderTextureHandle> reads,
    std::vector<RenderTextureHandle> writes,
    std::vector<std::string>& execution_order
  )
      : name_(std::move(name)), reads_(std::move(reads)),
        writes_(std::move(writes)), execution_order_(&execution_order) {}

  [[nodiscard]] auto name() const -> std::string override { return name_; }

  auto setup(RenderGraphBuilder& builder) -> void override {
    for (auto const& resource : reads_) {
      builder.read(resource);
    }
    for (auto const& resource : writes_) {
      builder.write(resource);
    }
  }

  auto execute(PassContext&, RenderGraphRuntime&) -> void override {
    execution_order_->push_back(name_);
  }

private:
  std::string name_;
  std::vector<RenderTextureHandle> reads_;
  std::vector<RenderTextureHandle> writes_;
  std::vector<std::string>* execution_order_;
};

auto add_recording_pass(
  RenderGraph& graph,
  std::string name,
  std::vector<RenderTextureHandle> reads,
  std::vector<RenderTextureHandle> writes,
  std::vector<std::string>& execution_order
) -> void {
  graph.add_pass(std::make_unique<RecordingPass>(
    std::move(name), std::move(reads), std::move(writes), execution_order
  ));
}

auto execute(RenderGraph& graph) -> void {
  gl::RenderState render_state;
  ExtractedView view{};
  ExtractedLights lights{};
  Time time{};
  Assets<Mesh> mesh_assets;
  Assets<Texture> texture_assets;
  ShaderCache shader_cache;
  SlotProviderRegistry slot_registry;
  OpaquePhase opaque_phase{};
  TransparentPhase transparent_phase{};
  ShadowPhase shadow_phase{};
  UiPhase ui_phase{};
  gl::ClusterConfig cluster_config{};
  gl::ClusteredLightingState cluster_state{};
  PassContext ctx{
    .marker = NonSendMarker{},
    .render_state = render_state,
    .framebuffer = {},
    .view = view,
    .lights = lights,
    .time = time,
    .mesh_assets = mesh_assets,
    .texture_assets = texture_assets,
    .shader_cache = shader_cache,
    .slot_registry = slot_registry,
    .opaque_phase = opaque_phase,
    .transparent_phase = transparent_phase,
    .shadow_phase = shadow_phase,
    .ui_phase = ui_phase,
    .cluster_config = cluster_config,
    .cluster_state = cluster_state,
  };
  graph.execute(ctx);
}

} // namespace

TEST(RenderGraphTest, SortsProducerBeforeConsumer) {
  RenderGraph graph;
  std::vector<std::string> order;
  auto const color = graph.import_backbuffer("color");

  add_recording_pass(graph, "consumer", {color}, {}, order);
  add_recording_pass(graph, "producer", {}, {color}, order);

  execute(graph);

  ASSERT_EQ(order.size(), 2U);
  EXPECT_EQ(order[0], "producer");
  EXPECT_EQ(order[1], "consumer");
}

TEST(RenderGraphTest, KeepsReadersBeforeLaterOverwrite) {
  RenderGraph graph;
  std::vector<std::string> order;
  auto const color = graph.import_backbuffer("color");

  add_recording_pass(graph, "first-writer", {}, {color}, order);
  add_recording_pass(graph, "reader", {color}, {}, order);
  add_recording_pass(graph, "overwrite", {}, {color}, order);

  execute(graph);

  ASSERT_EQ(order.size(), 3U);
  EXPECT_EQ(order[0], "first-writer");
  EXPECT_EQ(order[1], "reader");
  EXPECT_EQ(order[2], "overwrite");
}

TEST(RenderGraphTest, DebugNameDoesNotDefineIdentity) {
  RenderGraph graph;

  auto const first = graph.import_backbuffer("same-name");
  auto const second = graph.import_backbuffer("same-name");

  EXPECT_TRUE(first.is_valid());
  EXPECT_TRUE(second.is_valid());
  EXPECT_NE(first, second);
}

TEST(RenderGraphTest, BackbufferImportDoesNotAllocateTextureTarget) {
  RenderGraph graph;
  std::vector<std::string> order;
  auto const screen = graph.import_backbuffer("Screen");

  add_recording_pass(graph, "screen-writer", {}, {screen}, order);

  execute(graph);

  EXPECT_EQ(order.size(), 1U);
  EXPECT_FALSE(graph.runtime().get_texture(screen).ready());
}
