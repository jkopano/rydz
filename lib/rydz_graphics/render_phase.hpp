#pragma once

#include "render_extract.hpp"
#include "rydz_graphics/material/render_material.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace ecs {

namespace detail {
inline bool prepare_mesh(const Handle<Mesh> &handle,
                         Assets<Mesh> &mesh_assets) {
  auto *mesh = mesh_assets.get(handle);
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
    Mat4 world_transform = Mat4::sIdentity();
    f32 distance_to_camera = 0.0F;
  };

  std::vector<Item> items;

  void clear() { *this = ShadowPhase{}; }

  void queue(const ExtractedMeshes &meshes) {
    clear();
    for (const auto &item : meshes.items) {
      if (!item.casts_shadows) {
        continue;
      }
      items.push_back(Item{
          .mesh = item.mesh,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
      });
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
    Mat4 world_transform = Mat4::sIdentity();
    f32 distance_to_camera = 0.0F;
  };

  std::vector<Item> items;
  std::vector<Batch> batches;

  void clear() { *this = OpaquePhase{}; }

  void queue(const ExtractedMeshes &meshes) {
    clear();
    for (const auto &item : meshes.items) {
      if (item.transparent) {
        continue;
      }
      items.push_back(Item{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
      });
    }
  }

  void build_batches() {
    batches.clear();
    std::unordered_map<RenderBatchKey, usize> batch_index;

    for (const auto &item : items) {

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
          math::to_rl(item.world_transform));
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
    Mat4 world_transform = Mat4::sIdentity();
    f32 sort_key = 0.0F;
  };

  std::vector<Item> items;
  std::vector<Batch> batches;

  void clear() { *this = TransparentPhase{}; }

  void queue(const ExtractedMeshes &meshes) {
    clear();
    for (const auto &item : meshes.items) {
      if (!item.transparent) {
        continue;
      }
      items.push_back(Item{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .sort_key = item.distance_to_camera,
      });
    }

    std::ranges::sort(items, [](const Item &lhs, const Item &rhs) {
      return lhs.sort_key > rhs.sort_key;
    });
  }

  void build_batches() {
    batches.clear();

    for (const auto &item : items) {

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

  void clear() { *this = UiPhase{}; }

  void queue(const ExtractedUi &ui) {
    clear();
    for (const auto &item : ui.items) {
      items.push_back(Item{
          .texture = item.texture,
          .transform = item.transform,
          .tint = item.tint,
          .layer = item.layer,
      });
    }

    std::ranges::stable_sort(items, [](const Item &lhs, const Item &rhs) {
      return lhs.layer < rhs.layer;
    });
  }
};

struct Prepare {
  template <typename P> static void build_batches(ResMut<P> phase) {
    phase->build_batches();
  };

  static void prepare_meshes(Res<ExtractedMeshes> extracted,
                             ResMut<Assets<Mesh>> mesh_assets, NonSendMarker) {
    std::unordered_set<u32> seen;

    for (const auto &item : extracted->items) {
      if (!seen.insert(item.mesh.id).second) {
        continue;
      }

      detail::prepare_mesh(item.mesh, *mesh_assets);
    }
  }
};

} // namespace ecs
