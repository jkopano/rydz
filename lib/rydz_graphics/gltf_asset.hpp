#pragma once
#include "raymath.h"
#include "rydz_ecs/asset.hpp"
#include <cstring>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace ecs {

// Parsed fragment from a path like "model.glb#Scene0/Mesh1"
struct GltfFragment {
  enum class Type {
    Scene,
    MeshByName,
    MeshByIndex,
    MaterialByName,
    MaterialByIndex,
    ByName
  };
  Type type;
  std::string name;
  int index = -1;
};

// Parse fragment string (everything after #) into structured queries
// Supports:
//   #Scene0/Mesh1    -> scene "Scene0", mesh "Mesh1"
//   #Mesh1           -> mesh by name "Mesh1"
//   #Material0       -> material by name "Material0"
//   #3               -> mesh by index 3
inline GltfFragment parse_gltf_fragment(const std::string &fragment) {
  GltfFragment result;

  // Strip leading #
  std::string frag = fragment;
  if (!frag.empty() && frag[0] == '#')
    frag = frag.substr(1);

  if (frag.empty()) {
    result.type = GltfFragment::Type::Scene;
    return result;
  }

  auto slash = frag.find('/');
  if (slash != std::string::npos) {
    result.type = GltfFragment::Type::MeshByName;
    result.name = frag.substr(slash + 1);
    return result;
  }

  if (frag.rfind("Material", 0) == 0) {
    result.type = GltfFragment::Type::MaterialByName;
    result.name = frag;
    return result;
  }

  bool is_number = true;
  for (char c : frag) {
    if (!std::isdigit(c)) {
      is_number = false;
      break;
    }
  }

  if (is_number) {
    result.type = GltfFragment::Type::MeshByIndex;
    result.index = std::stoi(frag);
    return result;
  }

  result.type = GltfFragment::Type::ByName;
  result.name = frag;
  return result;
}

inline std::vector<std::string> split_fragments(const std::string &fragment) {
  std::vector<std::string> result;
  if (fragment.empty())
    return result;

  std::string current;
  for (size_t i = 0; i < fragment.size(); ++i) {
    if (fragment[i] == '|') {
      if (!current.empty())
        result.push_back(current);
      current.clear();
    } else {
      current += fragment[i];
    }
  }
  if (!current.empty())
    result.push_back(current);

  return result;
}

inline int find_mesh_in_scene(const GltfScene &scene,
                              const GltfFragment &frag) {
  switch (frag.type) {
  case GltfFragment::Type::MeshByIndex:
    if (frag.index >= 0 && frag.index < scene.meshCount)
      return frag.index;
    return -1;

  case GltfFragment::Type::MeshByName:
  case GltfFragment::Type::ByName:
    for (int i = 0; i < scene.meshCount; ++i) {
      if (std::strcmp(scene.meshes[i].name, frag.name.c_str()) == 0)
        return i;
    }
    return -1;

  default:
    return -1;
  }
}

inline int find_material_in_scene(const GltfScene &scene,
                                  const GltfFragment &frag) {
  if (frag.type == GltfFragment::Type::MaterialByName) {
    std::string num_part = frag.name.substr(8); // skip "Material"
    if (!num_part.empty()) {
      int idx = std::stoi(num_part);
      if (idx >= 0 && idx < scene.materialCount)
        return idx;
    }
  }
  return -1;
}

struct SceneRoot {
  std::vector<Handle<Mesh>> meshes;
  std::vector<Handle<Material>> materials;
};

class GltfLoader : public IAssetLoader {
public:
  std::vector<std::string> extensions() const override {
    return {"gltf", "glb"};
  }

  bool is_async() const override { return false; }

  std::any load(const std::vector<uint8_t> & /*data*/,
                const std::string &path) override {
    // For sync loaders, the path is passed directly
    return std::any(std::string(path));
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path_with_fragment = std::any_cast<std::string>(std::move(asset));

    // Split path and fragment
    auto hash_pos = path_with_fragment.find('#');
    std::string file_path = path_with_fragment;
    std::string fragment;
    if (hash_pos != std::string::npos) {
      file_path = path_with_fragment.substr(0, hash_pos);
      fragment = path_with_fragment.substr(hash_pos);
    }

    // Load scene (cache by file path)
    GltfScene *scene = get_or_load_scene(file_path);
    if (!scene || scene->meshCount == 0)
      return;

    if (fragment.empty()) {
      // No fragment: load as full Model (wrap scene into a Model)
      auto *model_assets = world.get_resource<Assets<Model>>();
      if (model_assets) {
        Model model = {0};
        model.transform = MatrixIdentity();
        model.meshCount = scene->meshCount;
        model.materialCount = scene->materialCount;

        // Deep copy meshes and upload to GPU
        model.meshes = (Mesh *)RL_CALLOC(model.meshCount, sizeof(Mesh));
        for (int i = 0; i < model.meshCount; i++) {
          model.meshes[i] = scene->meshes[i];
          UploadMesh(&model.meshes[i], false);
        }

        model.materials =
            (Material *)RL_CALLOC(model.materialCount, sizeof(Material));
        for (int i = 0; i < model.materialCount; i++)
          model.materials[i] = scene->materials[i];

        model.meshMaterial = (int *)RL_CALLOC(model.meshCount, sizeof(int));
        for (int i = 0; i < model.meshCount; i++)
          model.meshMaterial[i] = scene->meshMaterial[i];

        model_assets->set(Handle<Model>{handle_id}, model);
      }
    } else {
      // Parse fragments and resolve sub-assets
      auto frags = split_fragments(fragment);
      for (auto &frag_str : frags) {
        auto parsed = parse_gltf_fragment(frag_str);

        if (parsed.type == GltfFragment::Type::MaterialByName) {
          int mat_idx = find_material_in_scene(*scene, parsed);
          if (mat_idx >= 0) {
            auto *mat_assets = world.get_resource<Assets<Material>>();
            if (mat_assets) {
              mat_assets->set(Handle<Material>{handle_id},
                              scene->materials[mat_idx]);
            }
          }
        } else {
          // Try to find mesh
          int mesh_idx = find_mesh_in_scene(*scene, parsed);
          if (mesh_idx >= 0) {
            auto *mesh_assets = world.get_resource<Assets<Mesh>>();
            if (mesh_assets) {
              Mesh m = scene->meshes[mesh_idx];
              UploadMesh(&m, false);
              mesh_assets->set(Handle<Mesh>{handle_id}, m);
            }
          }
        }
      }
    }
  }

private:
  std::unordered_map<std::string, GltfScene> scene_cache_;

  GltfScene *get_or_load_scene(const std::string &path) {
    auto it = scene_cache_.find(path);
    if (it != scene_cache_.end())
      return &it->second;

    GltfScene scene = LoadGltfScene(path.c_str());
    if (scene.meshCount == 0)
      return nullptr;

    auto [inserted, _] = scene_cache_.emplace(path, scene);
    return &inserted->second;
  }
};

} // namespace ecs
