#pragma once

#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include <algorithm>

namespace ecs {

struct DebugOverlaySettings {
  using T = Resource;

  bool draw_fps = true;
  gl::Vec2 fps_position = {10.0f, 10.0f};
};

struct PipelineState {
  using T = Resource;

  gl::RenderTarget world_target{};
  u32 width = 0;
  u32 height = 0;

  PipelineState() = default;
  PipelineState(const PipelineState &) = delete;
  PipelineState &operator=(const PipelineState &) = delete;

  PipelineState(PipelineState &&other) noexcept
      : world_target(other.world_target), width(other.width),
        height(other.height) {
    other.world_target = gl::RenderTarget{};
    other.width = 0;
    other.height = 0;
  }

  PipelineState &operator=(PipelineState &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    unload();
    world_target = other.world_target;
    width = other.width;
    height = other.height;
    other.world_target = gl::RenderTarget{};
    other.width = 0;
    other.height = 0;
    return *this;
  }

  ~PipelineState() { unload(); }

  [[nodiscard]] bool ready() const { return world_target.ready(); }

  void ensure_target(u32 target_width, u32 target_height) {
    target_width = std::max(target_width, 1U);
    target_height = std::max(target_height, 1U);

    if (ready() && width == target_width && height == target_height) {
      return;
    }

    unload();
    world_target = gl::RenderTarget{target_width, target_height};
    width = target_width;
    height = target_height;

    if (world_target.texture.id != 0) {
      world_target.texture.set_filter(gl::TEXTURE_FILTER_BILINEAR);
    }
  }

  void unload() {
    if (world_target.id != 0) {
      world_target.unload();
      world_target = gl::RenderTarget{};
    }
    width = 0;
    height = 0;
  }
};

} // namespace ecs
