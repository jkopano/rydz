#pragma once
#include "render_extract.hpp"
#include "render_material.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace ecs {

struct ShadowPhaseItem {
  Handle<rl::Mesh> mesh{};
  Mat4 world_transform = Mat4::sIdentity();
  float distance_to_camera = 0.0f;
};

struct ShadowPhase {
  using T = Resource;
  std::vector<ShadowPhaseItem> items;

  void clear() { items.clear(); }
};

struct OpaquePhaseItem {
  Handle<rl::Mesh> mesh{};
  StandardMaterial material{};
  Mat4 world_transform = Mat4::sIdentity();
  float distance_to_camera = 0.0f;
};

struct OpaquePhase {
  using T = Resource;
  std::vector<OpaquePhaseItem> items;
  std::vector<OpaqueBatch> batches;

  void clear() {
    items.clear();
    batches.clear();
  }
};

struct TransparentPhaseItem {
  Handle<rl::Mesh> mesh{};
  StandardMaterial material{};
  Mat4 world_transform = Mat4::sIdentity();
  float sort_key = 0.0f;
};

struct TransparentPhase {
  using T = Resource;
  std::vector<TransparentPhaseItem> items;

  void clear() { items.clear(); }
};

struct UiPhaseItem {
  Handle<rl::Texture2D> texture{};
  Transform transform{};
  rl::Color tint = WHITE;
  i32 layer = 0;
};

struct UiPhase {
  using T = Resource;
  std::vector<UiPhaseItem> items;

  void clear() { items.clear(); }
};

using OverlayPhaseItem = UiPhaseItem;
using OverlayPhase = UiPhase;

struct RenderPhaseSystems {
  static void queue_shadow_phase(Res<ExtractedMeshes> meshes,
                                 ResMut<ShadowPhase> phase) {
    phase->clear();

    for (const auto &item : meshes->items) {
      if (!item.casts_shadows) {
        continue;
      }

      phase->items.push_back(ShadowPhaseItem{
          .mesh = item.mesh,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
      });
    }
  }

  static void queue_opaque_phase(Res<ExtractedMeshes> meshes,
                                 ResMut<OpaquePhase> phase) {
    phase->clear();

    for (const auto &item : meshes->items) {
      if (item.transparent) {
        continue;
      }

      phase->items.push_back(OpaquePhaseItem{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .distance_to_camera = item.distance_to_camera,
      });
    }
  }

  static void queue_transparent_phase(Res<ExtractedMeshes> meshes,
                                      ResMut<TransparentPhase> phase) {
    phase->clear();

    for (const auto &item : meshes->items) {
      if (!item.transparent) {
        continue;
      }

      phase->items.push_back(TransparentPhaseItem{
          .mesh = item.mesh,
          .material = item.material,
          .world_transform = item.world_transform,
          .sort_key = item.distance_to_camera,
      });
    }

    std::sort(
        phase->items.begin(), phase->items.end(),
        [](const TransparentPhaseItem &lhs, const TransparentPhaseItem &rhs) {
          return lhs.sort_key > rhs.sort_key;
        });
  }

  static void queue_ui_phase(Res<ExtractedUi> ui, ResMut<UiPhase> phase) {
    phase->clear();

    for (const auto &item : ui->items) {
      phase->items.push_back(UiPhaseItem{
          .texture = item.texture,
          .transform = item.transform,
          .tint = item.tint,
          .layer = item.layer,
      });
    }

    std::stable_sort(
        phase->items.begin(), phase->items.end(),
        [](const UiPhaseItem &lhs, const UiPhaseItem &rhs) {
          return lhs.layer < rhs.layer;
        });
  }

  static void queue_overlay_phase(Res<ExtractedOverlay> overlay,
                                  ResMut<OverlayPhase> phase) {
    queue_ui_phase(overlay, phase);
  }

  static void build_opaque_batches(ResMut<OpaquePhase> phase,
                                   ResMut<Assets<rl::Mesh>> mesh_assets,
                                   NonSendMarker marker) {
    detail::build_opaque_batches(*phase, *mesh_assets, marker);
  }

private:
  struct detail {
    static void build_opaque_batches(OpaquePhase &phase,
                                     Assets<rl::Mesh> &mesh_assets,
                                     NonSendMarker) {
      phase.batches.clear();
      std::unordered_map<RenderBatchKey, usize> batch_index;

      for (const auto &item : phase.items) {
        rl::Mesh *mesh = mesh_assets.get(item.mesh);
        if (!mesh || mesh->vertexCount <= 0 || mesh->vertices == nullptr) {
          continue;
        }

        if (mesh->vaoId == 0) {
          rl::UploadMesh(mesh, false);
        }

        RenderBatchKey key{};
        key.mesh = item.mesh;
        key.material = material_key(item.material);

        auto it = batch_index.find(key);
        if (it == batch_index.end()) {
          usize batch_slot = phase.batches.size();
          OpaqueBatch batch{};
          batch.key = key;
          batch.transforms.push_back(to_rl(item.world_transform));
          phase.batches.push_back(std::move(batch));
          batch_index.emplace(phase.batches.back().key, batch_slot);
          continue;
        }

        phase.batches[it->second].transforms.push_back(
            to_rl(item.world_transform));
      }
    }
  };
};

} // namespace ecs
