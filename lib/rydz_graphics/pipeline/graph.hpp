#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/mod.hpp"
#include "rydz_graphics/pipeline/pass_context.hpp"
#include "rydz_graphics/pipeline/phase.hpp"
#include "rydz_log/mod.hpp"
#include <memory>
#include <optional>
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
  bool transient = false;
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

class RenderGraphBuilder;
class RenderGraphRuntime;

class RenderPass {
public:
  virtual ~RenderPass() = default;
  [[nodiscard]] virtual auto name() const -> std::string { return {}; }

  virtual auto setup(RenderGraphBuilder& builder) -> void = 0;
  virtual auto execute(PassContext& ctx, RenderGraphRuntime& runtime) -> void = 0;
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

  auto execute(PassContext& ctx) -> void {
    if (!compiled_) {
      info("RenderGraph: compiling");
      compile();
    }

    ensure_resources(
      static_cast<u32>(ctx.framebuffer.width), static_cast<u32>(ctx.framebuffer.height)
    );

    for (auto& node : nodes_) {
      if (node.pass) {
        execute_node(node, ctx);
      }
    }
  }

  [[nodiscard]] auto runtime() const -> RenderGraphRuntime const& { return runtime_; }

  [[nodiscard]] auto debug_physical_slot(
    RenderTextureHandle handle, u32 default_w = 1, u32 default_h = 1
  ) const -> std::optional<usize> {
    auto const [logical_to_physical, _] = build_physical_plan(default_w, default_h);
    if (!handle.is_valid() || handle.id >= logical_to_physical.size()) {
      return std::nullopt;
    }

    usize const slot = logical_to_physical[handle.id];
    if (slot == INVALID_PHYSICAL_SLOT) {
      return std::nullopt;
    }
    return slot;
  }

  auto clear() -> void {
    passes_.clear();
    nodes_.clear();
    for (auto& target : runtime_.physical_targets) {
      target.unload();
    }
    runtime_.physical_targets.clear();
    runtime_.logical_to_physical.clear();
    resources_.clear();
    compiled_ = false;
  }

private:
  struct ResolvedTextureDesc {
    u32 width = 0;
    u32 height = 0;
    i32 format = gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    bool use_depth = true;
    bool hdr = false;

    auto operator==(ResolvedTextureDesc const&) const -> bool = default;
  };

  struct ResourceInfo {
    std::string debug_name;
    TextureDesc desc{};
    bool backbuffer = false;
    usize first_use = USIZE_MAX;
    usize last_use = 0;
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
      auto const slot = physical_slot(handle);
      if (slot != INVALID_PHYSICAL_SLOT && slot < physical_targets.size()) {
        return physical_targets[slot].texture;
      }
      static gl::Texture empty{};
      return empty;
    }

    [[nodiscard]] auto get_target(RenderTextureHandle handle) const
      -> gl::RenderTarget const& override {
      auto const slot = physical_slot(handle);
      if (slot != INVALID_PHYSICAL_SLOT && slot < physical_targets.size()) {
        return physical_targets[slot];
      }
      static gl::RenderTarget empty{};
      return empty;
    }

    std::vector<gl::RenderTarget> physical_targets;
    std::vector<usize> logical_to_physical;

  private:
    [[nodiscard]] auto physical_slot(RenderTextureHandle handle) const -> usize {
      if (!handle.is_valid() || handle.id >= logical_to_physical.size()) {
        return INVALID_PHYSICAL_SLOT;
      }
      return logical_to_physical[handle.id];
    }
  };

  static constexpr usize INVALID_PHYSICAL_SLOT = USIZE_MAX;
  std::vector<ResourceInfo> resources_;
  std::vector<std::unique_ptr<RenderPass>> passes_;
  std::vector<PassNode> nodes_;
  RuntimeImpl runtime_;
  bool compiled_ = false;

  [[nodiscard]] auto has_resource(RenderTextureHandle handle) const -> bool {
    return handle.is_valid() && handle.id < resources_.size();
  }

  [[nodiscard]] auto is_backbuffer(RenderTextureHandle handle) const -> bool {
    return has_resource(handle) && resources_[handle.id].backbuffer;
  }

  auto execute_node(PassNode& node, PassContext& ctx) -> void {

    for (auto const& output : node.outputs) {
      if (!is_backbuffer(output)) {
        runtime_.get_target(output).begin();
      }
    }

    struct TargetGuard {
      RuntimeImpl const& runtime;
      std::vector<RenderTextureHandle> const& outputs;
      std::vector<ResourceInfo> const& resources;

      ~TargetGuard() {
        for (auto const& output : outputs) {
          if (output.is_valid() && output.id < resources.size() &&
              !resources[output.id].backbuffer) {
            runtime.get_target(output).end();
          }
        }
      }
    } guard{.runtime = runtime_, .outputs = node.outputs, .resources = resources_};

    node.pass->execute(ctx, runtime_);
  }

  auto ensure_resources(u32 default_w, u32 default_h) -> void {
    auto const [logical_to_physical, physical_descs] =
      build_physical_plan(default_w, default_h);
    runtime_.logical_to_physical = logical_to_physical;

    for (usize index = physical_descs.size(); index < runtime_.physical_targets.size();
         ++index) {
      runtime_.physical_targets[index].unload();
    }
    runtime_.physical_targets.resize(physical_descs.size());

    for (usize index = 0; index < physical_descs.size(); ++index) {
      auto const& desc = physical_descs[index];
      auto& target = runtime_.physical_targets[index];

      if (target.ready() && target.texture.width == static_cast<i32>(desc.width) &&
          target.texture.height == static_cast<i32>(desc.height)) {
        continue;
      }

      target.unload();
      target = gl::RenderTarget::with_depth_texture(desc.width, desc.height, desc.format);
      if (target.ready()) {
        info(
          "RenderGraph: allocated physical target slot {} ({}x{}) [ID {}]",
          static_cast<int>(index),
          static_cast<int>(desc.width),
          static_cast<int>(desc.height),
          target.id
        );
        target.texture.set_filter(gl::TEXTURE_FILTER_BILINEAR);
      }
    }
  }

  [[nodiscard]] auto resolve_desc(
    ResourceInfo const& resource, u32 default_w, u32 default_h
  ) const -> ResolvedTextureDesc {
    return ResolvedTextureDesc{
      .width = resource.desc.width > 0 ? resource.desc.width : default_w,
      .height = resource.desc.height > 0 ? resource.desc.height : default_h,
      .format = resource.desc.format,
      .use_depth = resource.desc.use_depth,
      .hdr = resource.desc.hdr,
    };
  }

  [[nodiscard]] auto build_physical_plan(u32 default_w, u32 default_h) const
    -> std::pair<std::vector<usize>, std::vector<ResolvedTextureDesc>> {
    struct PhysicalSlot {
      ResolvedTextureDesc desc{};
      usize last_use = 0;
    };

    std::vector<usize> logical_to_physical(resources_.size(), INVALID_PHYSICAL_SLOT);
    std::vector<ResolvedTextureDesc> physical_descs;
    std::vector<PhysicalSlot> slots;

    for (usize resource_index = 0; resource_index < resources_.size(); ++resource_index) {
      auto const& resource = resources_[resource_index];
      if (resource.backbuffer) {
        continue;
      }

      ResolvedTextureDesc const desc = resolve_desc(resource, default_w, default_h);
      usize slot_index = INVALID_PHYSICAL_SLOT;
      bool const has_usage = resource.first_use != USIZE_MAX;

      if (resource.desc.transient && has_usage) {
        for (usize candidate = 0; candidate < slots.size(); ++candidate) {
          auto const& slot = slots[candidate];
          if (slot.desc == desc && slot.last_use < resource.first_use) {
            slot_index = candidate;
            break;
          }
        }
      }

      if (slot_index == INVALID_PHYSICAL_SLOT) {
        slot_index = slots.size();
        slots.push_back(PhysicalSlot{.desc = desc, .last_use = resource.last_use});
        physical_descs.push_back(desc);
      } else {
        slots[slot_index].last_use = resource.last_use;
      }

      logical_to_physical[resource_index] = slot_index;
    }

    return {std::move(logical_to_physical), std::move(physical_descs)};
  }

  auto compute_resource_lifetimes() -> void {
    for (auto& resource : resources_) {
      resource.first_use = USIZE_MAX;
      resource.last_use = 0;
    }

    auto record_use = [&](RenderTextureHandle handle, usize pass_index) -> void {
      if (!has_resource(handle)) {
        return;
      }

      auto& resource = resources_[handle.id];
      resource.first_use = std::min(resource.first_use, pass_index);
      resource.last_use = std::max(resource.last_use, pass_index);
    };

    for (usize index = 0; index < nodes_.size(); ++index) {
      for (auto const& input : nodes_[index].inputs) {
        record_use(input, index);
      }
      for (auto const& output : nodes_[index].outputs) {
        record_use(output, index);
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
    compute_resource_lifetimes();
  }
};

} // namespace ecs
