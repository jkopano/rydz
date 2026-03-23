#pragma once
#include "rydz_ecs/asset.hpp"
#include "render_config.hpp"
#include <absl/hash/hash.h>
#include <cstdint>
#include "rl.hpp"
#include <utility>
#include <vector>

namespace ecs {

struct MaterialKey {
  rl::Color base_color = WHITE;
  uint32_t texture_id = UINT32_MAX;
  uint32_t normal_id = UINT32_MAX;
  float metallic = 0.0f;
  float roughness = 0.5f;
  const rl::Shader *shader = nullptr;

  bool operator==(const MaterialKey &o) const {
    return base_color.r == o.base_color.r && base_color.g == o.base_color.g &&
           base_color.b == o.base_color.b && base_color.a == o.base_color.a &&
           texture_id == o.texture_id && normal_id == o.normal_id &&
           metallic == o.metallic && roughness == o.roughness &&
           shader == o.shader;
  }
};

struct RenderBatchKey {
  Handle<rl::Model> model{};
  int mesh_index = 0;
  int material_index = 0;
  MaterialKey material;
  RenderConfig rc{};
  bool has_rc = false;

  bool operator==(const RenderBatchKey &o) const {
    return model == o.model && mesh_index == o.mesh_index &&
           material_index == o.material_index && material == o.material &&
           has_rc == o.has_rc &&
           rc.depth.test == o.rc.depth.test &&
           rc.depth.write == o.rc.depth.write &&
           rc.depth.func == o.rc.depth.func && rc.blend == o.rc.blend &&
           rc.cull == o.rc.cull && rc.polygon_mode == o.rc.polygon_mode &&
           rc.wireframe == o.rc.wireframe;
  }
};

struct RenderBatch {
  RenderBatchKey key;
  std::vector<rl::Matrix> transforms;
};

struct RenderBatches {
  using Type = Resource;
  std::vector<RenderBatch> batches;

  void clear() { batches.clear(); }
};

template <typename H>
H AbslHashValue(H h, const MaterialKey &k) {
  return H::combine(std::move(h), k.base_color.r, k.base_color.g,
                    k.base_color.b, k.base_color.a, k.texture_id,
                    k.normal_id, k.metallic, k.roughness,
                    reinterpret_cast<uintptr_t>(k.shader));
}

template <typename H>
H AbslHashValue(H h, const RenderBatchKey &k) {
  return H::combine(std::move(h), k.model.id, k.mesh_index, k.material_index,
                    k.material, k.has_rc, k.rc.depth.test, k.rc.depth.write,
                    k.rc.depth.func, k.rc.blend, k.rc.cull, k.rc.polygon_mode,
                    k.rc.wireframe);
}

} // namespace ecs
