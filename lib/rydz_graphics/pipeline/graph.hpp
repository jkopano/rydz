#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/mod.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_log/mod.hpp"
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ecs {

struct TextureDesc {
  u32 width = 0;
  u32 height = 0;
  i32 format = gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  bool use_depth = true;
  bool hdr = false;
};

struct RenderGraphTexture {};
using RenderTextureHandle = Handle<RenderGraphTexture>;

struct DebugOverlaySettings;
struct EnvironmentRenderer;
struct ExtractedLights;
struct ExtractedView;
struct RenderExecutionState;
struct ShaderCache;
struct SlotProviderRegistry;

struct FrameResources {
  NonSendMarker marker;
  gl::RenderState& render_state;
  u32 framebuffer_width = 1280;
  u32 framebuffer_height = 720;

  RenderExecutionState* execution_state{};
  ShadowPhase const* shadow_phase{};
  OpaquePhase const* opaque_phase{};
  TransparentPhase const* transparent_phase{};
  UiPhase const* ui_phase{};

  Assets<Mesh> const* mesh_assets{};
  Assets<Texture> const* texture_assets{};
  ShaderCache* shader_cache{};
  SlotProviderRegistry const* slot_registry{};
  EnvironmentRenderer const* environment_renderer{};
  ExtractedView const* view{};
  ExtractedLights const* lights{};
  ClusterConfig const* cluster_config{};
  ClusteredLightingState* cluster_state{};
  Time const* time{};
};

class RenderGraphBuilder;
class RenderGraphRuntime;

class RenderPass {
public:
  virtual ~RenderPass() = default;

  virtual auto setup(RenderGraphBuilder& builder) -> void = 0;
  virtual auto execute(FrameResources& frame, RenderGraphRuntime& runtime) -> void = 0;
};

class RenderGraphBuilder {
public:
  virtual ~RenderGraphBuilder() = default;

  virtual auto read(RenderTextureHandle handle) -> void = 0;
  virtual auto write(RenderTextureHandle handle) -> void = 0;
};

class RenderGraphRuntime {
public:
  virtual ~RenderGraphRuntime() = default;

  [[nodiscard]] virtual auto get_texture(RenderTextureHandle handle) const
    -> gl::Texture const& = 0;
  [[nodiscard]] virtual auto get_target(RenderTextureHandle handle) const
    -> gl::RenderTarget const& = 0;
};

class RenderGraph {
public:
  using T = Resource;

  auto create_texture(TextureDesc desc = {}, std::string debug_name = {})
    -> RenderTextureHandle {
    RenderTextureHandle handle{static_cast<u32>(resources_.size())};
    resources_.push_back(
      ResourceInfo{
        .debug_name = std::move(debug_name),
        .desc = desc,
        .backbuffer = false,
      }
    );
    return handle;
  }

  auto import_backbuffer(std::string debug_name = {}) -> RenderTextureHandle {
    RenderTextureHandle handle{static_cast<u32>(resources_.size())};
    resources_.push_back(
      ResourceInfo{
        .debug_name = std::move(debug_name),
        .desc = {},
        .backbuffer = true,
      }
    );
    return handle;
  }

  auto add_pass(std::unique_ptr<RenderPass> pass) -> void {
    passes_.push_back(std::move(pass));
    compiled_ = false;
  }

  template <typename PassT, typename... Args> auto add_pass(Args&&... args) -> void {
    add_pass(std::make_unique<PassT>(std::forward<Args>(args)...));
  }

  auto compile() -> void {
    if (compiled_) {
      return;
    }

    nodes_.clear();

    for (auto& pass : passes_) {
      PassNode node;
      node.pass = std::move(pass);

      BuilderImpl builder(node);
      node.pass->setup(builder);

      nodes_.push_back(std::move(node));
    }

    sort_nodes();

    passes_.clear();
    compiled_ = true;
  }

  auto execute(FrameResources& frame) -> void {
    if (!compiled_) {
      info("RenderGraph: compiling");
      compile();
    }

    ensure_resources(frame.framebuffer_width, frame.framebuffer_height);

    for (auto& node : nodes_) {
      if (node.pass) {
        node.pass->execute(frame, runtime_);
      }
    }
  }

  [[nodiscard]] auto runtime() const -> RenderGraphRuntime const& { return runtime_; }

  auto clear() -> void {
    passes_.clear();
    nodes_.clear();
    for (auto& target : runtime_.targets) {
      target.unload();
    }
    runtime_.targets.clear();
    resources_.clear();
    compiled_ = false;
  }

private:
  struct ResourceInfo {
    std::string debug_name;
    TextureDesc desc{};
    bool backbuffer = false;
  };

  struct PassNode {
    std::unique_ptr<RenderPass> pass;
    std::vector<RenderTextureHandle> inputs;
    std::vector<RenderTextureHandle> outputs;
  };

  class BuilderImpl : public RenderGraphBuilder {
  public:
    explicit BuilderImpl(PassNode& node) : node_(node) {}

    auto read(RenderTextureHandle handle) -> void override {
      node_.inputs.push_back(handle);
    }

    auto write(RenderTextureHandle handle) -> void override {
      node_.outputs.push_back(handle);
    }

  private:
    PassNode& node_;
  };

  class RuntimeImpl : public RenderGraphRuntime {
  public:
    [[nodiscard]] auto get_texture(RenderTextureHandle handle) const
      -> gl::Texture const& override {
      if (handle.is_valid() && handle.id < targets.size()) {
        return targets[handle.id].texture;
      }
      static gl::Texture empty{};
      return empty;
    }

    [[nodiscard]] auto get_target(RenderTextureHandle handle) const
      -> gl::RenderTarget const& override {
      if (handle.is_valid() && handle.id < targets.size()) {
        return targets[handle.id];
      }
      static gl::RenderTarget empty{};
      return empty;
    }

    std::vector<gl::RenderTarget> targets;
  };

  std::vector<ResourceInfo> resources_;
  std::vector<std::unique_ptr<RenderPass>> passes_;
  std::vector<PassNode> nodes_;
  RuntimeImpl runtime_;
  bool compiled_ = false;

  [[nodiscard]] auto has_resource(RenderTextureHandle handle) const -> bool {
    return handle.is_valid() && handle.id < resources_.size();
  }

  auto ensure_resources(u32 default_w, u32 default_h) -> void {
    if (runtime_.targets.size() < resources_.size()) {
      runtime_.targets.resize(resources_.size());
    }

    for (u32 index = 0; index < resources_.size(); ++index) {
      auto const& resource = resources_[index];
      if (resource.backbuffer) {
        continue;
      }

      auto const& desc = resource.desc;
      auto& target = runtime_.targets[index];
      u32 const w = desc.width > 0 ? desc.width : default_w;
      u32 const h = desc.height > 0 ? desc.height : default_h;

      if (target.ready() && target.texture.width == static_cast<i32>(w) &&
          target.texture.height == static_cast<i32>(h)) {
        continue;
      }

      target.unload();
      target = gl::RenderTarget(w, h);
      if (target.ready()) {
        info(
          "RenderGraph: allocated target '{}' ({}x{}) [ID {}]",
          resource.debug_name.empty() ? "<unnamed>" : resource.debug_name,
          w,
          h,
          target.id
        );
        target.texture.set_filter(gl::TEXTURE_FILTER_BILINEAR);
      }
    }
  }

  auto sort_nodes() -> void {
    struct ResourceUsage {
      std::vector<usize> readers;
      std::vector<usize> writers;
    };

    usize const node_count = nodes_.size();
    std::vector<ResourceUsage> usages(resources_.size());
    for (usize index = 0; index < node_count; ++index) {
      for (auto const& input : nodes_[index].inputs) {
        if (!has_resource(input)) {
          warn("RenderGraph: pass reads invalid resource '{}'", input.id);
          continue;
        }
        usages[input.id].readers.push_back(index);
      }
      for (auto const& output : nodes_[index].outputs) {
        if (!has_resource(output)) {
          warn("RenderGraph: pass writes invalid resource '{}'", output.id);
          continue;
        }
        usages[output.id].writers.push_back(index);
      }
    }

    std::vector<std::vector<usize>> edges(node_count);
    std::vector<std::unordered_set<usize>> edge_sets(node_count);
    std::vector<usize> indegree(node_count, 0);

    auto add_edge = [&](usize from, usize to) -> void {
      if (from == to) {
        return;
      }
      auto [_, inserted] = edge_sets[from].insert(to);
      if (!inserted) {
        return;
      }
      edges[from].push_back(to);
      ++indegree[to];
    };

    for (auto const& usage : usages) {
      for (usize writer_index = 1; writer_index < usage.writers.size(); ++writer_index) {
        add_edge(usage.writers[writer_index - 1], usage.writers[writer_index]);
      }

      for (usize reader : usage.readers) {
        usize previous_writer = USIZE_MAX;
        usize next_writer = USIZE_MAX;

        for (usize writer : usage.writers) {
          if (writer < reader) {
            previous_writer = writer;
            continue;
          }
          if (writer > reader) {
            next_writer = writer;
            break;
          }
        }

        if (previous_writer != USIZE_MAX) {
          add_edge(previous_writer, reader);
          if (next_writer != USIZE_MAX) {
            add_edge(reader, next_writer);
          }
        } else if (next_writer != USIZE_MAX) {
          add_edge(next_writer, reader);
        }
      }
    }

    std::vector<usize> sorted_indices;
    sorted_indices.reserve(node_count);
    std::vector<bool> emitted(node_count, false);

    for (usize emitted_count = 0; emitted_count < node_count;) {
      usize selected = USIZE_MAX;
      for (usize index = 0; index < node_count; ++index) {
        if (!emitted[index] && indegree[index] == 0) {
          selected = index;
          break;
        }
      }

      if (selected == USIZE_MAX) {
        warn(
          "RenderGraph: dependency cycle detected; preserving remaining pass "
          "order"
        );
        for (usize index = 0; index < node_count; ++index) {
          if (!emitted[index]) {
            sorted_indices.push_back(index);
            emitted[index] = true;
            ++emitted_count;
          }
        }
        break;
      }

      sorted_indices.push_back(selected);
      emitted[selected] = true;
      ++emitted_count;

      for (usize dependent : edges[selected]) {
        --indegree[dependent];
      }
    }

    std::vector<PassNode> sorted_nodes;
    sorted_nodes.reserve(node_count);
    for (usize index : sorted_indices) {
      sorted_nodes.push_back(std::move(nodes_[index]));
    }
    nodes_ = std::move(sorted_nodes);
  }
};

} // namespace ecs
