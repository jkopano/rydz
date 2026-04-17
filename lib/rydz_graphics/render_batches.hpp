#pragma once

#include "hash.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include <cstdint>
#include <functional>
#include <vector>

namespace ecs {

using gl::Mesh;

struct RenderBatchKey {
  Handle<Mesh> mesh{};
  CompiledMaterial material;

  auto operator==(const RenderBatchKey &o) const -> bool {
    return mesh == o.mesh && material == o.material;
  }
};

} // namespace ecs

namespace std {

template <> struct hash<ecs::RenderBatchKey> {
  auto operator()(const ecs::RenderBatchKey &k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<uint32_t>{}(k.mesh.id));
    rydz::hash_combine(seed, std::hash<ecs::CompiledMaterial>{}(k.material));
    return seed;
  }
};

} // namespace std
