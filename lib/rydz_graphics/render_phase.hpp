#pragma once

#include "render_extract.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include "rydz_graphics/render_batches.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace ecs {

namespace detail {
inline auto prepare_mesh(Handle<Mesh> const& handle, Assets<Mesh>& mesh_assets)
  -> bool {
  auto* mesh = mesh_assets.get(handle);
  if ((mesh == nullptr) || mesh->vertex_count() <= 0 ||
      mesh->vertex_data() == nullptr) {
    return false;
  }
  if (!mesh->uploaded()) {
    mesh->upload(false);
  }
  return true;
}
} // namespace detail

struct ShadowPhase {
  using T = Resource;

  struct Item {
    Handle<Mesh> mesh{};
    Mat4 world_transform = Mat4::IDENTITY;
    f32 distance_to_camera = 0.0F;
  };

  std::vector<Item> items;

  auto clear() -> void { *this = ShadowPhase{}; }

  auto queue(ExtractedMeshes const& meshes) -> void {
    clear();
    for (auto const& item : meshes.items) {
      if (!item.casts_shadows) {
        continue;
      }
      items.push_back(
        Item{
          .mesh = item.mesh,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
        }
      );
    }
  }
};

struct OpaquePhase {
  using T = Resource;

  struct Batch {
    RenderBatchKey key;
    std::vector<gl::Matrix> transforms;
  };

  struct Item {
    Handle<Mesh> mesh{};
    CompiledMaterial material{};
    Mat4 world_transform = Mat4::IDENTITY;
    f32 distance_to_camera = 0.0F;
  };

  std::vector<Item> items;
  std::vector<Batch> batches;

  auto clear() -> void { *this = OpaquePhase{}; }

  auto queue(ExtractedMeshes const& meshes) -> void {
    clear();
    for (auto const& item : meshes.items) {
      if (item.transparent) {
        continue;
      }
      items.push_back(
        Item{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
        }
      );
    }
  }

  auto build_batches() -> void {
    batches.clear();
    std::unordered_map<RenderBatchKey, usize> batch_index;

    for (auto const& item : items) {

      RenderBatchKey key{};
      key.mesh = item.mesh;
      key.material = item.material;

      auto it = batch_index.find(key);
      if (it == batch_index.end()) {
        usize batch_slot = batches.size();
        Batch batch{};
        batch.key = key;
        batch.transforms.push_back(math::to_rl(item.world_transform));
        batches.push_back(std::move(batch));
        batch_index.emplace(batches.back().key, batch_slot);
        continue;
      }

      batches[it->second].transforms.push_back(
        math::to_rl(item.world_transform)
      );
    }
  }
};

struct TransparentPhase {
  using T = Resource;

  struct Batch {
    RenderBatchKey key;
    std::vector<gl::Matrix> transforms;
  };

  struct Item {
    Handle<Mesh> mesh{};
    CompiledMaterial material{};
    Mat4 world_transform = Mat4::IDENTITY;
    f32 sort_key = 0.0F;
  };

  std::vector<Item> items;
  std::vector<Batch> batches;

  auto clear() -> void { *this = TransparentPhase{}; }

  auto queue(ExtractedMeshes const& meshes) -> void {
    clear();
    for (auto const& item : meshes.items) {
      if (!item.transparent) {
        continue;
      }
      items.push_back(
        Item{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .sort_key = item.distance_to_camera,
        }
      );
    }

    std::ranges::sort(items, [](Item const& lhs, Item const& rhs) -> bool {
      return lhs.sort_key > rhs.sort_key;
    });
  }

  auto build_batches() -> void {
    batches.clear();

    for (auto const& item : items) {

      RenderBatchKey key{};
      key.mesh = item.mesh;
      key.material = item.material;

      if (batches.empty() || !(batches.back().key == key)) {
        Batch batch{};
        batch.key = key;
        batch.transforms.push_back(math::to_rl(item.world_transform));
        batches.push_back(std::move(batch));
        continue;
      }

      batches.back().transforms.push_back(math::to_rl(item.world_transform));
    }
  }
};

struct UiPhase {
  using T = Resource;

  struct Item {
    Handle<Texture> texture{};
    Transform transform{};
    Color tint = kWhite;
    i32 layer = 0;
  };

  std::vector<Item> items;

  auto clear() -> void { *this = UiPhase{}; }

  auto queue(ExtractedUi const& ui) -> void {
    clear();
    for (auto const& item : ui.items) {
      items.push_back(
        Item{
          .texture = item.texture,
          .transform = item.transform,
          .tint = item.tint,
          .layer = item.layer,
        }
      );
    }

    std::ranges::stable_sort(
      items, [](Item const& lhs, Item const& rhs) -> bool {
        return lhs.layer < rhs.layer;
      }
    );
  }
};

struct Prepare {
  template <typename P> static void build_batches(ResMut<P> phase) {
    phase->build_batches();
  };

  static auto prepare_meshes(
    Res<ExtractedMeshes> extracted,
    ResMut<Assets<Mesh>> mesh_assets,
    NonSendMarker
  ) -> void {
    std::unordered_set<u32> seen;

    for (auto const& item : extracted->items) {
      if (!seen.insert(item.mesh.id).second) {
        continue;
      }

      detail::prepare_mesh(item.mesh, *mesh_assets);
    }
  }
};

} // namespace ecs
