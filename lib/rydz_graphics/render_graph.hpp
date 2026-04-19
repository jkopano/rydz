#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/mod.hpp"
#include "rydz_graphics/render_phase.hpp"
#include "rydz_log/mod.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ecs {

struct TextureDesc {
  u32 width = 0;
  u32 height = 0;
  i32 format = gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  bool use_depth = true;
  bool hdr = false;

  static auto screen(Res<Window> const& window) -> TextureDesc {
    return {
      .width = static_cast<u32>(window->width),
      .height = static_cast<u32>(window->height),
    };
  }
};

using RenderResourceHandle = std::string;

struct RenderPassContext {
  World& world;
  gl::RenderState& render_state;
  NonSendMarker marker;
};

class RenderGraphBuilder;
class RenderGraphRuntime;

class RenderPass {
public:
  virtual ~RenderPass() = default;
  [[nodiscard]] virtual auto name() const -> std::string = 0;

  virtual auto setup(RenderGraphBuilder& builder) -> void = 0;
  virtual auto execute(RenderPassContext& ctx, RenderGraphRuntime& runtime)
    -> void = 0;
};

class RenderGraphBuilder {
public:
  virtual ~RenderGraphBuilder() = default;

  virtual auto read(RenderResourceHandle const& name) -> void = 0;
  virtual auto write(RenderResourceHandle const& name, TextureDesc const& desc)
    -> void = 0;
  virtual auto write_backbuffer(RenderResourceHandle const& name) -> void = 0;
};

class RenderGraphRuntime {
public:
  virtual ~RenderGraphRuntime() = default;

  [[nodiscard]] virtual auto get_texture(RenderResourceHandle const& name) const
    -> gl::Texture const& = 0;
  [[nodiscard]] virtual auto get_target(RenderResourceHandle const& name) const
    -> gl::RenderTarget const& = 0;
};

class RenderGraph {
public:
  using T = Resource;

  auto add_pass(std::unique_ptr<RenderPass> pass) -> void {
    passes_.push_back(std::move(pass));
    compiled_ = false;
  }

  template <typename PassT, typename... Args>
  auto add_pass(Args&&... args) -> void {
    add_pass(std::make_unique<PassT>(std::forward<Args>(args)...));
  }

  auto compile(World& world) -> void {
    if (compiled_) {
      return;
    }

    nodes_.clear();
    resource_descriptors_.clear();
    backbuffer_resources_.clear();

    for (auto& pass : passes_) {
      PassNode node;
      node.pass = std::move(pass);

      BuilderImpl builder(node);
      node.pass->setup(builder);

      for (auto const& [name, desc] : builder.descriptors) {
        resource_descriptors_[name] = desc;
      }
      for (auto const& name : builder.backbuffers) {
        backbuffer_resources_.insert(name);
      }

      nodes_.push_back(std::move(node));
    }

    // proste sortowanie topologiczne (obecnie zakładamy kolejność dodawania,
    // nie ma pełnego sortowania)
    // sort_nodes();

    passes_.clear();
    compiled_ = true;
  }

  auto execute(
    World& world, gl::RenderState& render_state, NonSendMarker marker
  ) -> void {
    if (!compiled_) {
      info("RenderGraph: compiling");
      compile(world);
    }

    ensure_resources(world);

    RenderPassContext ctx{
      .world = world, .render_state = render_state, .marker = marker
    };

    for (auto& node : nodes_) {
      if (node.pass) {
        node.pass->execute(ctx, runtime_);
      }
    }
  }

  [[nodiscard]] auto runtime() const -> RenderGraphRuntime const& {
    return runtime_;
  }

  auto clear() -> void {
    passes_.clear();
    nodes_.clear();
    runtime_.targets.clear();
    resource_descriptors_.clear();
    backbuffer_resources_.clear();
    compiled_ = false;
  }

private:
  struct PassNode {
    std::unique_ptr<RenderPass> pass;
    std::vector<RenderResourceHandle> inputs;
    std::vector<RenderResourceHandle> outputs;
  };

  class BuilderImpl : public RenderGraphBuilder {
  public:
    explicit BuilderImpl(PassNode& node) : node_(node) {}

    auto read(RenderResourceHandle const& name) -> void override {
      node_.inputs.push_back(name);
    }

    auto write(RenderResourceHandle const& name, TextureDesc const& desc)
      -> void override {
      node_.outputs.push_back(name);
      descriptors[name] = desc;
    }

    auto write_backbuffer(RenderResourceHandle const& name) -> void override {
      node_.outputs.push_back(name);
      backbuffers.insert(name);
    }

    std::unordered_map<RenderResourceHandle, TextureDesc> descriptors;
    std::unordered_set<RenderResourceHandle> backbuffers;

  private:
    PassNode& node_;
  };

  class RuntimeImpl : public RenderGraphRuntime {
  public:
    [[nodiscard]] auto get_texture(RenderResourceHandle const& name) const
      -> gl::Texture const& override {
      auto it = targets.find(name);
      if (it != targets.end()) {
        return it->second.texture;
      }
      static gl::Texture empty{};
      return empty;
    }

    [[nodiscard]] auto get_target(RenderResourceHandle const& name) const
      -> gl::RenderTarget const& override {
      return targets.at(name);
    }

    std::unordered_map<RenderResourceHandle, gl::RenderTarget> targets;
  };

  std::vector<std::unique_ptr<RenderPass>> passes_;
  std::vector<PassNode> nodes_;
  RuntimeImpl runtime_;
  std::unordered_map<RenderResourceHandle, TextureDesc> resource_descriptors_;
  std::unordered_set<RenderResourceHandle> backbuffer_resources_;
  bool compiled_ = false;

  auto ensure_resources(World& world) -> void {
    auto* window = world.get_resource<Window>();
    u32 const default_w = window ? static_cast<u32>(window->width) : 1280;
    u32 const default_h = window ? static_cast<u32>(window->height) : 720;

    for (auto const& [name, desc] : resource_descriptors_) {
      auto& target = runtime_.targets[name];
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
          name,
          w,
          h,
          target.id
        );
        target.texture.set_filter(gl::TEXTURE_FILTER_BILINEAR);
      }
    }
  }

  auto sort_nodes() -> void {
    // TODO: Pelny sort topologiczny
  }
};

} // namespace ecs
