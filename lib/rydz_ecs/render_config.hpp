#pragma once
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

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
      rlEnableDepthTest();
    } else {
      rlDisableDepthTest();
    }
    if (depth.write) {
      rlEnableDepthMask();
    } else {
      rlDisableDepthMask();
    }

    switch (cull) {
    case CullMode::Back:
      rlEnableBackfaceCulling();
      rlSetCullFace(RL_CULL_FACE_BACK);
      break;
    case CullMode::Front:
      rlEnableBackfaceCulling();
      rlSetCullFace(RL_CULL_FACE_FRONT);
      break;
    case CullMode::None:
      rlDisableBackfaceCulling();
      break;
    }

    if (wireframe || polygon_mode == PolygonMode::Line) {
      rlEnableWireMode();
    }

    switch (blend) {
    case BlendMode::Alpha:
      rlSetBlendMode(RL_BLEND_ALPHA);
      break;
    case BlendMode::Additive:
      rlSetBlendMode(RL_BLEND_ADDITIVE);
      break;
    case BlendMode::Multiplied:
      rlSetBlendMode(RL_BLEND_MULTIPLIED);
      break;
    case BlendMode::Custom:
      break;
    }
  }

  static void restore_defaults() {
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_BACK);
    rlDisableWireMode();
    rlSetBlendMode(RL_BLEND_ALPHA);
  }
};

} // namespace ecs
