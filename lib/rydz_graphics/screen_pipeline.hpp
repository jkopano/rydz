#pragma once

#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include <algorithm>

namespace ecs {

struct PostProcessSettings {
  using T = Resource;

  bool enabled = true;
  float exposure = 1.0f;
  float contrast = 1.05f;
  float saturation = 1.0f;
  float vignette = 0.18f;
  float grain = 0.015f;
};

struct DebugOverlaySettings {
  using T = Resource;

  bool draw_fps = true;
  rl::Vector2 fps_position = {10.0f, 10.0f};
};

struct ScreenPipelineState {
  using T = Resource;

  rl::RenderTexture2D world_target{};
  int width = 0;
  int height = 0;

  ScreenPipelineState() = default;
  ScreenPipelineState(const ScreenPipelineState &) = delete;
  ScreenPipelineState &operator=(const ScreenPipelineState &) = delete;

  ScreenPipelineState(ScreenPipelineState &&other) noexcept
      : world_target(other.world_target), width(other.width),
        height(other.height) {
    other.world_target = {};
    other.width = 0;
    other.height = 0;
  }

  ScreenPipelineState &operator=(ScreenPipelineState &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    unload();
    world_target = other.world_target;
    width = other.width;
    height = other.height;
    other.world_target = {};
    other.width = 0;
    other.height = 0;
    return *this;
  }

  ~ScreenPipelineState() { unload(); }

  bool ready() const { return world_target.id != 0; }

  void ensure_target(int target_width, int target_height) {
    target_width = std::max(target_width, 1);
    target_height = std::max(target_height, 1);

    if (ready() && width == target_width && height == target_height) {
      return;
    }

    unload();
    world_target = rl::LoadRenderTexture(target_width, target_height);
    width = target_width;
    height = target_height;

    if (world_target.texture.id != 0) {
      rl::SetTextureFilter(world_target.texture, TEXTURE_FILTER_BILINEAR);
    }
  }

  void unload() {
    if (world_target.id != 0) {
      rl::UnloadRenderTexture(world_target);
      world_target = {};
    }
    width = 0;
    height = 0;
  }
};

} // namespace ecs
