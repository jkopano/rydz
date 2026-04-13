#pragma once

#include "math.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_graphics/assets/types.hpp"
#include "rydz_graphics/mesh3d.hpp"
#include "rydz_graphics/transform.hpp"
#include <array>
#include <string>
#include <vector>

namespace ecs {

struct SceneMaterial {
  std::string name;
  Handle<Material> material;
};

struct ScenePrimitive {
  Handle<Mesh> mesh;
  usize material_index = 0;
  i32 skin_index = -1;
  Transform local_transform{};
  std::string name;
};

struct SceneNode {
  std::string name;
  Transform local_transform{};
  i32 parent = -1;
  std::vector<usize> children;
  std::vector<ScenePrimitive> primitives;
  i32 bone_index = -1;
};

struct SceneBoneData {
  std::string name;
  i32 parent = -1;
  i32 node_index = -1;
  Transform bind_pose{};
};

struct SceneSkin {
  std::string name;
  i32 skeleton_root_node = -1;
  std::vector<i32> bone_indices;
  std::vector<std::array<float, 16>> inverse_bind_matrices;
};

struct Scene {
  std::vector<SceneNode> nodes;
  std::vector<usize> roots;
  std::vector<SceneMaterial> materials;
  std::vector<SceneBoneData> bones;
  std::vector<SceneSkin> skins;
};

struct SceneRoot {
  Handle<Scene> scene;
};

struct SceneOwned {
  Entity root;
};

struct SceneNodeInstance {
  Entity root;
  usize node_index = 0;
};

struct ScenePrimitiveInstance {
  Entity root;
  usize node_index = 0;
  usize primitive_index = 0;
};

struct SceneJoint {
  usize bone_index = 0;
};

struct SceneSkinBinding {
  i32 skin_index = -1;
};

struct SceneInstance {
  Handle<Scene> scene;
  std::vector<Entity> node_entities;
  std::vector<std::vector<Entity>> primitive_entities;
  std::vector<Entity> owned_entities;
};

} // namespace ecs
