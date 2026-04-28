#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include <vector>

namespace ecs {

using gl::Mesh;
using gl::Texture;

struct RenderCommand {
  Handle<Mesh> mesh{};
  usize material_index{};
  std::vector<rl::Matrix> instances;
  AABox world_bounds{};
  f32 sort_key = 0.0F;

  auto add_instance(Mat4 const& transform) -> void {
    instances.push_back(math::to_rl(transform));
  }
};

template <typename Tag> struct RenderPhase {
  using T = Resource;

  std::vector<RenderCommand> commands;

  auto clear() -> void { commands.clear(); }
};

struct OpaqueTag {};
struct TransparentTag {};
struct ShadowTag {};

using OpaquePhase = RenderPhase<OpaqueTag>;
using TransparentPhase = RenderPhase<TransparentTag>;
using ShadowPhase = RenderPhase<ShadowTag>;

struct UiPhase {
  using T = Resource;

  struct Item {
    Handle<Texture> texture{};
    Transform transform{};
    Color tint = Color::WHITE;
    i32 layer = 0;
  };

  std::vector<Item> items;

  auto clear() -> void { items.clear(); }
};

} // namespace ecs
