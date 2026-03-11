#pragma once
#include "rl.hpp"

namespace ecs {

enum class DepthFunc {
  Always,
  Never,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Equal,
  NotEqual,
};

struct Depth {
  bool test = true;
  bool write = true;
  DepthFunc func = DepthFunc::Less;
};

enum class BlendMode {
  Alpha,
  Additive,
  Multiplied,
  Custom,
};

enum class CullMode {
  None,
  Back,
  Front,
};

enum class PolygonMode {
  Fill,
  Line,
  Point,
};

struct RenderConfig {
  Depth depth;
  BlendMode blend = BlendMode::Alpha;
  CullMode cull = CullMode::Back;
  PolygonMode polygon_mode = PolygonMode::Fill;
  bool wireframe = false;

  void apply() const {

    if (depth.test) {
      rl::rlEnableDepthTest();
    } else {
      rl::rlDisableDepthTest();
    }
    if (depth.write) {
      rl::rlEnableDepthMask();
    } else {
      rl::rlDisableDepthMask();
    }

    switch (cull) {
    case CullMode::Back:
      rl::rlEnableBackfaceCulling();
      rl::rlSetCullFace(RL_CULL_FACE_BACK);
      break;
    case CullMode::Front:
      rl::rlEnableBackfaceCulling();
      rl::rlSetCullFace(RL_CULL_FACE_FRONT);
      break;
    case CullMode::None:
      rl::rlDisableBackfaceCulling();
      break;
    }

    if (wireframe || polygon_mode == PolygonMode::Line) {
      rl::rlEnableWireMode();
    }

    switch (blend) {
    case BlendMode::Alpha:
      rl::rlSetBlendMode(RL_BLEND_ALPHA);
      break;
    case BlendMode::Additive:
      rl::rlSetBlendMode(RL_BLEND_ADDITIVE);
      break;
    case BlendMode::Multiplied:
      rl::rlSetBlendMode(RL_BLEND_MULTIPLIED);
      break;
    case BlendMode::Custom:
      break;
    }
  }

  static void restore_defaults() {
    rl::rlEnableDepthTest();
    rl::rlEnableDepthMask();
    rl::rlEnableBackfaceCulling();
    rl::rlSetCullFace(RL_CULL_FACE_BACK);
    rl::rlDisableWireMode();
    rl::rlSetBlendMode(RL_BLEND_ALPHA);
  }
};

} // namespace ecs
