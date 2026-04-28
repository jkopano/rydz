#pragma once

#include "hash.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/material/material3d.hpp"
#include <cstdint>
#include <functional>
#include <typeindex>
#include <vector>

namespace ecs {

using gl::Mesh;

struct RenderMaterialKey {
  std::type_index asset_type{typeid(void)};
  u32 id = UINT32_MAX;

  auto operator==(RenderMaterialKey const&) const -> bool = default;
};

template <typename M> auto render_material_key(Handle<M> material) -> RenderMaterialKey {
  return RenderMaterialKey{
    .asset_type = std::type_index(typeid(bare_t<M>)),
    .id = material.id,
  };
}

struct RenderBatchKey {
  Handle<Mesh> mesh{};
  RenderMaterialKey material{};

  auto operator==(RenderBatchKey const& o) const -> bool {
    return mesh == o.mesh && material == o.material;
  }
};

} // namespace ecs

namespace std {

template <> struct hash<ecs::RenderMaterialKey> {
  auto operator()(ecs::RenderMaterialKey const& k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, k.asset_type.hash_code());
    rydz::hash_combine(seed, std::hash<uint32_t>{}(k.id));
    return seed;
  }
};

template <> struct hash<ecs::RenderBatchKey> {
  auto operator()(ecs::RenderBatchKey const& k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<uint32_t>{}(k.mesh.id));
    rydz::hash_combine(seed, std::hash<ecs::RenderMaterialKey>{}(k.material));
    return seed;
  }
};

} // namespace std
