#pragma once
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/scene_graph.hpp"
#include <external/cgltf.h>
#include <raymath.h>
#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ecs {

namespace detail {

inline std::string strip_gltf_fragment(const std::string &path) {
  auto hash = path.find('#');
  if (hash == std::string::npos) {
    return path;
  }
  return path.substr(0, hash);
}

inline Transform transform_from_cgltf_node(const cgltf_node &node) {
  Transform transform{};

  if (node.has_translation) {
    transform.translation =
        Vec3(node.translation[0], node.translation[1], node.translation[2]);
  }

  if (node.has_rotation) {
    transform.rotation = Quat(node.rotation[0], node.rotation[1],
                              node.rotation[2], node.rotation[3]);
  }

  if (node.has_scale) {
    transform.scale = Vec3(node.scale[0], node.scale[1], node.scale[2]);
  }

  if (node.has_matrix) {
    rl::Matrix matrix = {
        node.matrix[0],  node.matrix[1],  node.matrix[2],  node.matrix[3],
        node.matrix[4],  node.matrix[5],  node.matrix[6],  node.matrix[7],
        node.matrix[8],  node.matrix[9],  node.matrix[10], node.matrix[11],
        node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15],
    };

    rl::Vector3 translation{};
    rl::Quaternion rotation{};
    rl::Vector3 scale{};
    MatrixDecompose(matrix, &translation, &rotation, &scale);

    transform.translation =
        Vec3(translation.x, translation.y, translation.z);
    transform.rotation = Quat(rotation.x, rotation.y, rotation.z, rotation.w);
    transform.scale = Vec3(scale.x, scale.y, scale.z);
  }

  return transform;
}

inline Transform transform_from_matrix(const Mat4 &matrix) {
  rl::Vector3 translation{};
  rl::Quaternion rotation{};
  rl::Vector3 scale{};
  MatrixDecompose(math::to_rl(matrix), &translation, &rotation, &scale);

  Transform transform;
  transform.translation = Vec3(translation.x, translation.y, translation.z);
  transform.rotation = Quat(rotation.x, rotation.y, rotation.z, rotation.w);
  transform.scale = Vec3(scale.x, scale.y, scale.z);
  return transform;
}

inline bool has_texture(const rl::Texture2D &texture) { return texture.id > 0; }

inline Handle<rl::Texture2D>
transfer_texture(Assets<rl::Texture2D> &textures, const rl::Texture2D &texture,
                 std::unordered_map<unsigned int, Handle<rl::Texture2D>>
                     &texture_cache) {
  if (!has_texture(texture)) {
    return {};
  }

  auto cached = texture_cache.find(texture.id);
  if (cached != texture_cache.end()) {
    return cached->second;
  }

  auto handle = textures.add(texture);
  texture_cache.emplace(texture.id, handle);
  return handle;
}

inline SceneMaterial
material_from_rl_material(const rl::Material &material,
                          Assets<rl::Texture2D> &textures,
                          std::unordered_map<unsigned int, Handle<rl::Texture2D>>
                              &texture_cache,
                          const std::string &name = {}) {
  SceneMaterial scene_material;
  scene_material.name = name;

  if (material.maps) {
    scene_material.material.base_color =
        material.maps[MATERIAL_MAP_DIFFUSE].color;
    scene_material.material.emissive_color =
        material.maps[MATERIAL_MAP_EMISSION].color;
    scene_material.material.texture = transfer_texture(
        textures, material.maps[MATERIAL_MAP_DIFFUSE].texture, texture_cache);
    scene_material.material.normal_map = transfer_texture(
        textures, material.maps[MATERIAL_MAP_NORMAL].texture, texture_cache);
    scene_material.material.metallic_map =
        transfer_texture(textures, material.maps[MATERIAL_MAP_METALNESS].texture,
                         texture_cache);
    scene_material.material.roughness_map =
        transfer_texture(textures, material.maps[MATERIAL_MAP_ROUGHNESS].texture,
                         texture_cache);
    scene_material.material.occlusion_map =
        transfer_texture(textures, material.maps[MATERIAL_MAP_OCCLUSION].texture,
                         texture_cache);
    scene_material.material.emissive_map =
        transfer_texture(textures, material.maps[MATERIAL_MAP_EMISSION].texture,
                         texture_cache);
    scene_material.material.metallic =
        material.maps[MATERIAL_MAP_METALNESS].value;
    scene_material.material.roughness =
        material.maps[MATERIAL_MAP_ROUGHNESS].value;
    scene_material.material.normal_scale =
        material.maps[MATERIAL_MAP_NORMAL].value;
    scene_material.material.occlusion_strength =
        material.maps[MATERIAL_MAP_OCCLUSION].value;
  }

  return scene_material;
}

} // namespace detail

class SceneLoader : public IAssetLoader {
public:
  std::vector<std::string> extensions() const override {
    return {"gltf", "glb"};
  }

  bool is_async() const override { return false; }

  std::any load(const std::vector<uint8_t> & /*data*/,
                const std::string &path) override {
    return std::any(std::string(path));
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path_with_fragment = std::any_cast<std::string>(std::move(asset));
    std::string file_path = detail::strip_gltf_fragment(path_with_fragment);

    auto *scene_assets = world.get_resource<Assets<Scene>>();
    auto *mesh_assets = world.get_resource<Assets<rl::Mesh>>();
    auto *texture_assets = world.get_resource<Assets<rl::Texture2D>>();
    if (!scene_assets || !mesh_assets || !texture_assets) {
      return;
    }

    rl::Model model = rl::LoadModel(file_path.c_str());
    if (model.meshCount <= 0 || model.meshes == nullptr) {
      return;
    }

    cgltf_options options{};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&options, file_path.c_str(), &data) !=
        cgltf_result_success) {
      rl::UnloadModel(model);
      return;
    }

    if (cgltf_load_buffers(&options, data, file_path.c_str()) !=
        cgltf_result_success) {
      cgltf_free(data);
      rl::UnloadModel(model);
      return;
    }

    Scene scene;
    scene.nodes.resize(data->nodes_count);

    std::unordered_map<unsigned int, Handle<rl::Texture2D>> texture_cache;
    const usize material_count = static_cast<usize>(std::max(model.materialCount, 1));
    scene.materials.reserve(material_count);
    for (int i = 0; i < static_cast<int>(material_count); ++i) {
      std::string material_name;
      if (i > 0 && static_cast<usize>(i - 1) < data->materials_count &&
          data->materials[i - 1].name) {
        material_name = data->materials[i - 1].name;
      }
      if (model.materials && i < model.materialCount) {
        scene.materials.push_back(detail::material_from_rl_material(
            model.materials[i], *texture_assets, texture_cache, material_name));
      } else {
        SceneMaterial material;
        material.name = std::move(material_name);
        scene.materials.push_back(std::move(material));
      }
    }

    if (scene.materials.empty()) {
      scene.materials.push_back(SceneMaterial{});
    }

    std::vector<std::vector<ScenePrimitive>> node_primitives(data->nodes_count);
    int model_mesh_index = 0;
    for (cgltf_size node_index = 0; node_index < data->nodes_count;
         ++node_index) {
      cgltf_node &node = data->nodes[node_index];
      if (!node.mesh) {
        continue;
      }

      for (cgltf_size primitive_index = 0;
           primitive_index < node.mesh->primitives_count; ++primitive_index) {
        const cgltf_primitive &primitive = node.mesh->primitives[primitive_index];
        if (primitive.type != cgltf_primitive_type_triangles) {
          continue;
        }
        if (model_mesh_index >= model.meshCount) {
          break;
        }

        Handle<rl::Mesh> mesh_handle = mesh_assets->add(model.meshes[model_mesh_index]);
        model.meshes[model_mesh_index] = rl::Mesh{};

        usize material_index = 0;
        if (model.meshMaterial && model_mesh_index < model.meshCount &&
            model.meshMaterial[model_mesh_index] >= 0) {
          material_index = static_cast<usize>(model.meshMaterial[model_mesh_index]);
        }
        material_index =
            std::min(material_index, scene.materials.size() - 1);

        ScenePrimitive scene_primitive;
        scene_primitive.mesh = mesh_handle;
        scene_primitive.material_index = material_index;
        scene_primitive.skin_index =
            node.skin ? static_cast<i32>(node.skin - data->skins) : -1;
        if (node.name) {
          scene_primitive.name = node.name;
        } else if (node.mesh->name) {
          scene_primitive.name = node.mesh->name;
        }

        node_primitives[node_index].push_back(std::move(scene_primitive));
        ++model_mesh_index;
      }
    }

    std::unordered_map<std::string, i32> bone_index_by_name;
    bone_index_by_name.reserve(model.skeleton.boneCount);
    scene.bones.reserve(model.skeleton.boneCount);
    for (int bone_index = 0; bone_index < model.skeleton.boneCount; ++bone_index) {
      SceneBoneData bone;
      bone.name = model.skeleton.bones[bone_index].name;
      bone.parent = model.skeleton.bones[bone_index].parent;

      if (model.skeleton.bindPose) {
        const auto &bind_pose = model.skeleton.bindPose[bone_index];
        bone.bind_pose.translation = Vec3(bind_pose.translation.x,
                                          bind_pose.translation.y,
                                          bind_pose.translation.z);
        bone.bind_pose.rotation = Quat(bind_pose.rotation.x, bind_pose.rotation.y,
                                       bind_pose.rotation.z, bind_pose.rotation.w);
        bone.bind_pose.scale =
            Vec3(bind_pose.scale.x, bind_pose.scale.y, bind_pose.scale.z);
      }

      bone_index_by_name.emplace(bone.name, bone_index);
      scene.bones.push_back(std::move(bone));
    }

    for (cgltf_size node_index = 0; node_index < data->nodes_count;
         ++node_index) {
      const cgltf_node &node = data->nodes[node_index];
      auto &scene_node = scene.nodes[node_index];
      scene_node.name = node.name ? node.name : "";
      scene_node.local_transform = detail::transform_from_cgltf_node(node);
      scene_node.parent = node.parent ? static_cast<i32>(node.parent - data->nodes)
                                      : -1;
      scene_node.primitives = std::move(node_primitives[node_index]);

      for (cgltf_size child_index = 0; child_index < node.children_count;
           ++child_index) {
        scene_node.children.push_back(
            static_cast<usize>(node.children[child_index] - data->nodes));
      }

      if (!node.parent) {
        scene.roots.push_back(node_index);
      }

      if (node.name) {
        auto bone_it = bone_index_by_name.find(node.name);
        if (bone_it != bone_index_by_name.end()) {
          scene_node.bone_index = bone_it->second;
          scene.bones[bone_it->second].node_index = static_cast<i32>(node_index);
        }
      }
    }

    std::vector<Mat4> node_world_transforms(scene.nodes.size(),
                                            Mat4::sIdentity());
    std::vector<bool> node_world_ready(scene.nodes.size(), false);
    std::function<Mat4(usize)> compute_node_world = [&](usize node_index) {
      if (node_world_ready[node_index]) {
        return node_world_transforms[node_index];
      }

      Mat4 local = scene.nodes[node_index].local_transform.compute_matrix();
      if (scene.nodes[node_index].parent >= 0) {
        local =
            compute_node_world(static_cast<usize>(scene.nodes[node_index].parent)) *
            local;
      }

      node_world_transforms[node_index] = local;
      node_world_ready[node_index] = true;
      return local;
    };

    for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
      Mat4 node_world = compute_node_world(node_index);
      Transform primitive_local =
          detail::transform_from_matrix(node_world.Inversed());

      for (auto &primitive : scene.nodes[node_index].primitives) {
        primitive.local_transform = primitive_local;
      }
    }

    scene.skins.reserve(data->skins_count);
    for (cgltf_size skin_index = 0; skin_index < data->skins_count;
         ++skin_index) {
      const cgltf_skin &skin = data->skins[skin_index];
      SceneSkin scene_skin;
      scene_skin.name = skin.name ? skin.name : "";
      scene_skin.skeleton_root_node =
          skin.skeleton ? static_cast<i32>(skin.skeleton - data->nodes) : -1;
      scene_skin.bone_indices.reserve(skin.joints_count);

      for (cgltf_size joint_index = 0; joint_index < skin.joints_count;
           ++joint_index) {
        const cgltf_node *joint = skin.joints[joint_index];
        i32 bone_index = -1;
        if (joint && joint->name) {
          auto it = bone_index_by_name.find(joint->name);
          if (it != bone_index_by_name.end()) {
            bone_index = it->second;
          }
        }
        scene_skin.bone_indices.push_back(bone_index);
      }

      if (skin.inverse_bind_matrices) {
        scene_skin.inverse_bind_matrices.resize(skin.inverse_bind_matrices->count);
        for (cgltf_size matrix_index = 0;
             matrix_index < skin.inverse_bind_matrices->count; ++matrix_index) {
          std::array<float, 16> values{};
          cgltf_accessor_read_float(skin.inverse_bind_matrices, matrix_index,
                                    values.data(), values.size());
          scene_skin.inverse_bind_matrices[matrix_index] = values;
        }
      }

      scene.skins.push_back(std::move(scene_skin));
    }

    scene_assets->set(Handle<Scene>{handle_id}, std::move(scene));

    cgltf_free(data);
    rl::UnloadModel(model);
  }
};

} // namespace ecs
