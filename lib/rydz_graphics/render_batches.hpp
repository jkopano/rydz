#pragma once
#include "render_config.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <cstdint>
#include <functional>
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

} // namespace ecs

namespace {
inline void hash_combine(std::size_t &seed, std::size_t value) noexcept {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}
} // namespace

namespace std {

template <> struct hash<ecs::MaterialKey> {
  size_t operator()(const ecs::MaterialKey &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<unsigned char>{}(k.base_color.r));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.base_color.g));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.base_color.b));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.base_color.a));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.texture_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.normal_id));
    ::hash_combine(seed, std::hash<float>{}(k.metallic));
    ::hash_combine(seed, std::hash<float>{}(k.roughness));
    ::hash_combine(seed, std::hash<const rl::Shader *>{}(k.shader));
    return seed;
  }
};

template <> struct hash<ecs::RenderBatchKey> {
  size_t operator()(const ecs::RenderBatchKey &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<uint32_t>{}(k.model.id));
    ::hash_combine(seed, std::hash<int>{}(k.mesh_index));
    ::hash_combine(seed, std::hash<int>{}(k.material_index));
    ::hash_combine(seed, std::hash<ecs::MaterialKey>{}(k.material));
    ::hash_combine(seed, std::hash<bool>{}(k.has_rc));
    ::hash_combine(seed, std::hash<bool>{}(k.rc.depth.test));
    ::hash_combine(seed, std::hash<bool>{}(k.rc.depth.write));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.rc.depth.func)));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.rc.blend)));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.rc.cull)));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.rc.polygon_mode)));
    ::hash_combine(seed, std::hash<bool>{}(k.rc.wireframe));
    return seed;
  }
};

} // namespace std
