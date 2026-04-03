#pragma once
#include "material3d.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace ecs {

struct RenderBatchKey {
  Handle<rl::Mesh> mesh{};
  MaterialDescriptor material;

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

template <> struct hash<ecs::ShaderProgramSpec> {
  size_t operator()(const ecs::ShaderProgramSpec &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<std::string>{}(k.vertex_path));
    ::hash_combine(seed, std::hash<std::string>{}(k.fragment_path));
    return seed;
  }
};

template <> struct hash<ecs::ShaderUniformValue> {
  size_t operator()(const ecs::ShaderUniformValue &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<std::string>{}(k.name));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.type)));
    ::hash_combine(seed, std::hash<int>{}(k.count));
    for (float value : k.float_data) {
      ::hash_combine(seed, std::hash<float>{}(value));
    }
    for (int value : k.int_data) {
      ::hash_combine(seed, std::hash<int>{}(value));
    }
    return seed;
  }
};

template <> struct hash<ecs::MaterialMapBinding> {
  size_t operator()(const ecs::MaterialMapBinding &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<int>{}(k.map_type));
    ::hash_combine(seed, std::hash<uint32_t>{}(k.texture.id));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.color.r));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.color.g));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.color.b));
    ::hash_combine(seed, std::hash<unsigned char>{}(k.color.a));
    ::hash_combine(seed, std::hash<float>{}(k.value));
    ::hash_combine(seed, std::hash<bool>{}(k.has_texture));
    ::hash_combine(seed, std::hash<bool>{}(k.has_color));
    ::hash_combine(seed, std::hash<bool>{}(k.has_value));
    return seed;
  }
};

template <> struct hash<ecs::MaterialFlags> {
  size_t operator()(const ecs::MaterialFlags &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<bool>{}(k.transparent));
    ::hash_combine(seed, std::hash<bool>{}(k.casts_shadows));
    ::hash_combine(seed, std::hash<bool>{}(k.double_sided));
    return seed;
  }
};

template <> struct hash<ecs::MaterialDescriptor> {
  size_t operator()(const ecs::MaterialDescriptor &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<ecs::ShaderProgramSpec>{}(k.shader));
    ::hash_combine(seed, std::hash<ecs::MaterialFlags>{}(k.flags));
    ::hash_combine(seed, std::hash<int>{}(static_cast<int>(k.shading_model)));
    for (const auto &map : k.maps) {
      ::hash_combine(seed, std::hash<ecs::MaterialMapBinding>{}(map));
    }
    for (const auto &uniform : k.uniforms) {
      ::hash_combine(seed, std::hash<ecs::ShaderUniformValue>{}(uniform));
    }
    return seed;
  }
};

template <> struct hash<ecs::RenderBatchKey> {
  size_t operator()(const ecs::RenderBatchKey &k) const noexcept {
    size_t seed = 0;
    ::hash_combine(seed, std::hash<uint32_t>{}(k.mesh.id));
    ::hash_combine(seed, std::hash<ecs::MaterialDescriptor>{}(k.material));
    return seed;
  }
};

} // namespace std
