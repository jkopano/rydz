#pragma once

#include "render_extract.hpp"
#include "render_material.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace ecs {

struct ShadowPhaseItem {
  Handle<Mesh> mesh{};
  Mat4 world_transform = Mat4::sIdentity();
  float distance_to_camera = 0.0f;
};

struct ShadowPhase {
  using T = Resource;
  std::vector<ShadowPhaseItem> items;

  void clear() { items.clear(); }
};

struct OpaquePhaseItem {
  Handle<Mesh> mesh{};
  CompiledMaterial material{};
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
  Handle<Mesh> mesh{};
  CompiledMaterial material{};
  Mat4 world_transform = Mat4::sIdentity();
  float sort_key = 0.0f;
};

struct TransparentPhase {
  using T = Resource;
  std::vector<TransparentPhaseItem> items;
  std::vector<TransparentBatch> batches;

  void clear() {
    items.clear();
    batches.clear();
  }
};

struct UiPhaseItem {
  Handle<Texture> texture{};
  Transform transform{};
  Color tint = kWhite;
  i32 layer = 0;
};

struct UiPhase {
  using T = Resource;
  std::vector<UiPhaseItem> items;

  void clear() { items.clear(); }
};

struct RenderPhaseSystems {
  struct Queue {
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

      std::stable_sort(phase->items.begin(), phase->items.end(),
                       [](const UiPhaseItem &lhs, const UiPhaseItem &rhs) {
                         return lhs.layer < rhs.layer;
                       });
    }

    static void queue_overlay_phase(Res<ExtractedUi> overlay,
                                    ResMut<UiPhase> phase) {
      queue_ui_phase(overlay, phase);
    }
  };

  struct Prepare {
    static void build_opaque_batches(ResMut<OpaquePhase> phase,
                                     ResMut<Assets<Mesh>> mesh_assets,
                                     NonSendMarker) {
      detail::build_opaque_batches(*phase, *mesh_assets, {});
    }

    static void build_transparent_batches(ResMut<TransparentPhase> phase,
                                          ResMut<Assets<Mesh>> mesh_assets,
                                          NonSendMarker) {
      detail::build_transparent_batches(*phase, *mesh_assets, {});
    }

  private:
    struct detail {
      static bool prepare_mesh(const Handle<Mesh> &handle,
                               Assets<Mesh> &mesh_assets) {
        auto *mesh = mesh_assets.get(handle);
        if (!mesh || gl::mesh_vertex_count(*mesh) <= 0 ||
            gl::mesh_vertices(*mesh) == nullptr) {
          return false;
        }

        if (!gl::mesh_uploaded(*mesh)) {
          gl::upload_mesh(*mesh, false);
        }

        return true;
      }

      static void build_opaque_batches(OpaquePhase &phase,
                                       Assets<Mesh> &mesh_assets,
                                       NonSendMarker) {
        phase.batches.clear();
        std::unordered_map<RenderBatchKey, usize> batch_index;

        for (const auto &item : phase.items) {
          if (!prepare_mesh(item.mesh, mesh_assets)) {
            continue;
          }

          RenderBatchKey key{};
          key.mesh = item.mesh;
          key.material = item.material;

          auto it = batch_index.find(key);
          if (it == batch_index.end()) {
            usize batch_slot = phase.batches.size();
            OpaqueBatch batch{};
            batch.key = key;
            batch.transforms.push_back(gl::to_matrix(item.world_transform));
            phase.batches.push_back(std::move(batch));
            batch_index.emplace(phase.batches.back().key, batch_slot);
            continue;
          }

          phase.batches[it->second].transforms.push_back(
              gl::to_matrix(item.world_transform));
        }
      }

      static void build_transparent_batches(TransparentPhase &phase,
                                            Assets<Mesh> &mesh_assets,
                                            NonSendMarker) {
        phase.batches.clear();

        for (const auto &item : phase.items) {
          if (!prepare_mesh(item.mesh, mesh_assets)) {
            continue;
          }

          RenderBatchKey key{};
          key.mesh = item.mesh;
          key.material = item.material;

          if (phase.batches.empty() || !(phase.batches.back().key == key)) {
            TransparentBatch batch{};
            batch.key = key;
            batch.transforms.push_back(gl::to_matrix(item.world_transform));
            phase.batches.push_back(std::move(batch));
            continue;
          }

          phase.batches.back().transforms.push_back(
              gl::to_matrix(item.world_transform));
        }
      }
    };
  };
};

} // namespace ecs
