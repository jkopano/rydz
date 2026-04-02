#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace ecs {

struct MaterialKey {
  rl::Color base_color = WHITE;
  rl::Color emissive_color = {0, 0, 0, 0};
  uint32_t texture_id = UINT32_MAX;
  uint32_t normal_id = UINT32_MAX;
  uint32_t metallic_id = UINT32_MAX;
  uint32_t roughness_id = UINT32_MAX;
  uint32_t occlusion_id = UINT32_MAX;
  uint32_t emissive_id = UINT32_MAX;
  float metallic = -1.0f;
  float roughness = -1.0f;
  float normal_scale = -1.0f;
  float occlusion_strength = -1.0f;
  const rl::Shader *shader = nullptr;

  bool operator==(const MaterialKey &o) const {
    return base_color.r == o.base_color.r && base_color.g == o.base_color.g &&
           base_color.b == o.base_color.b && base_color.a == o.base_color.a &&
           emissive_color.r == o.emissive_color.r &&
           emissive_color.g == o.emissive_color.g &&
           emissive_color.b == o.emissive_color.b &&
           emissive_color.a == o.emissive_color.a &&
           texture_id == o.texture_id && normal_id == o.normal_id &&
           metallic_id == o.metallic_id && roughness_id == o.roughness_id &&
           occlusion_id == o.occlusion_id && emissive_id == o.emissive_id &&
           metallic == o.metallic && roughness == o.roughness &&
           normal_scale == o.normal_scale &&
           occlusion_strength == o.occlusion_strength && shader == o.shader;
  }
};

struct RenderBatchKey {
  Handle<rl::Mesh> mesh{};
  MaterialKey material;

  bool operator==(const RenderBatchKey &o) const {
    return mesh == o.mesh && material == o.material;
  }
};

struct OpaqueBatch {
  RenderBatchKey key;
  std::vector<rl::Matrix> transforms;
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
    ::hash_combine(seed, std::hash<unsigned char>{}(k.emissive_color.r));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.emissive_color.g));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.emissive_color.b));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.emissive_color.a));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.texture_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.normal_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.metallic_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.roughness_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.occlusion_id));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.emissive_id));
    ::hash_combine(seed, std::hash<float>{}(k.metallic));
    ::hash_combine(seed, std::hash<float>{}(k.roughness));
    ::hash_combine(seed, std::hash<float>{}(k.normal_scale));
    ::hash_combine(seed, std::hash<float>{}(k.occlusion_strength));
    ::hash_combine(seed, std::hash<const rl::Shader *>{}(k.shader));
    return seed;
  }
};

template <> struct hash<ecs::RenderBatchKey> {
  size_t operator()(const ecs::RenderBatchKey &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<uint32_t>{}(k.mesh.id));
    ::hash_combine(seed, std::hash<ecs::MaterialKey>{}(k.material));
    return seed;
  }
};

} // namespace std
