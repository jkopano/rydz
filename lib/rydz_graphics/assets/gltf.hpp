#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/scene_graph.hpp"
#include "rydz_graphics/gl/resources.hpp"
#include "rydz_graphics/material/standard_material.hpp"
#include "rydz_graphics/transform.hpp"
#include <algorithm>
#include <array>
#include <external/cgltf.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ecs {

namespace detail {

inline auto strip_gltf_fragment(std::string const& path) -> std::string {
  auto hash = path.find('#');
  if (hash == std::string::npos) {
    return path;
  }
  return path.substr(0, hash);
}

inline auto transform_from_cgltf_node(cgltf_node const& node) -> Transform {
  Transform transform{};

  if (node.has_translation != 0) {
    transform.translation =
      Vec3(node.translation[0], node.translation[1], node.translation[2]);
  }

  if (node.has_rotation != 0) {
    transform.rotation = Quat(
      node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]
    );
  }

  if (node.has_scale) {
    transform.scale = Vec3(node.scale[0], node.scale[1], node.scale[2]);
  }

  if (node.has_matrix) {
    rl::Matrix matrix = {
      node.matrix[0],
      node.matrix[4],
      node.matrix[8],
      node.matrix[12],
      node.matrix[1],
      node.matrix[5],
      node.matrix[9],
      node.matrix[13],
      node.matrix[2],
      node.matrix[6],
      node.matrix[10],
      node.matrix[14],
      node.matrix[3],
      node.matrix[7],
      node.matrix[11],
      node.matrix[15],
    };

    transform = math::from_rl(matrix).decompose();
  }

  return transform;
}

inline auto transform_from_matrix(Mat4 const& matrix) -> Transform {
  return matrix.decompose();
}

inline auto apply_gltf_material_properties(
  CompiledMaterial& material_desc, cgltf_material const* material
) -> void {
  static constexpr auto DEFAULT_TRANSPARENT_MARGIN = 0.001F;
  static constexpr auto DEFAULT_ALPHACUTOFF = 0.5F;

  if (!material) {
    return;
  }

  if (material->unlit) {
    material_desc.shader =
      ShaderSpec::from("res/shaders/basic.vert", "res/shaders/basic.frag");
    material_desc.slots.clear();
  }

  material_desc.double_sided = material->double_sided;

  switch (material->alpha_mode) {
  case cgltf_alpha_mode_blend:
    material_desc.render_method = RenderMethod::Transparent;
    material_desc.casts_shadows = false;
    material_desc.alpha_cutoff = DEFAULT_TRANSPARENT_MARGIN;
    break;
  case cgltf_alpha_mode_mask:
    material_desc.render_method = RenderMethod::AlphaCutout;
    material_desc.casts_shadows = true;
    material_desc.alpha_cutoff = material->alpha_cutoff > 0.0F
                                   ? material->alpha_cutoff
                                   : DEFAULT_ALPHACUTOFF;
    break;
  case cgltf_alpha_mode_opaque:
  default:
    material_desc.render_method = RenderMethod::Opaque;
    material_desc.casts_shadows = true;
    break;
  }
}

inline auto transfer_texture(
  Assets<Texture>& textures,
  gl::Texture const& texture,
  std::unordered_map<unsigned int, Handle<Texture>>& texture_cache
) -> Handle<Texture> {
  if (!texture.ready()) {
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

inline auto material_from_backend_material(
  gl::Material const& material,
  Assets<Texture>& textures,
  Assets<Material>& materials,
  std::unordered_map<unsigned int, Handle<Texture>>& texture_cache,
  std::string const& name = {},
  cgltf_material const* gltf_material = nullptr
) -> SceneMaterial {
  SceneMaterial scene_material;
  scene_material.name = name;
  StandardMaterial material_value;

  if (material.maps != nullptr) {
    auto* maps = material.maps;
    material_value.base_color =
      maps[material_map_index(MaterialMap::Albedo)].color;
    material_value.emissive_color =
      maps[material_map_index(MaterialMap::Emission)].color;
    material_value.texture = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Albedo)].texture,
      texture_cache
    );
    material_value.normal_map = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Normal)].texture,
      texture_cache
    );
    material_value.metallic_map = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Metalness)].texture,
      texture_cache
    );
    material_value.roughness_map = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Roughness)].texture,
      texture_cache
    );
    material_value.occlusion_map = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Occlusion)].texture,
      texture_cache
    );
    material_value.emissive_map = transfer_texture(
      textures,
      maps[material_map_index(MaterialMap::Emission)].texture,
      texture_cache
    );
    material_value.metallic =
      maps[material_map_index(MaterialMap::Metalness)].value;
    material_value.roughness =
      maps[material_map_index(MaterialMap::Roughness)].value;
    material_value.normal_scale =
      maps[material_map_index(MaterialMap::Normal)].value;
    material_value.occlusion_strength =
      maps[material_map_index(MaterialMap::Occlusion)].value;
  }

  CompiledMaterial compiled = Material{material_value}.compiled;
  apply_gltf_material_properties(compiled, gltf_material);
  scene_material.material = materials.add(Material{std::move(compiled)});

  return scene_material;
}

} // namespace detail

class SceneLoader : public IAssetLoader {
public:
  auto extensions() const -> std::vector<std::string> override {
    return {"gltf", "glb"};
  }

  auto is_async() const -> bool override { return false; }

  auto load(std::vector<uint8_t> const& /*data*/, std::string const& path)
    -> std::any override {
    return {std::string(path)};
  }

  auto insert_into_world(World& world, uint32_t handle_id, std::any asset)
    -> void override {
    auto path_with_fragment = std::any_cast<std::string>(std::move(asset));
    std::string file_path = detail::strip_gltf_fragment(path_with_fragment);

    auto* scene_assets = world.get_resource<Assets<Scene>>();
    auto* mesh_assets = world.get_resource<Assets<Mesh>>();
    auto* texture_assets = world.get_resource<Assets<Texture>>();
    auto* material_assets = world.get_resource<Assets<Material>>();
    if ((scene_assets == nullptr) || (mesh_assets == nullptr) ||
        (texture_assets == nullptr) || (material_assets == nullptr)) {
      return;
    }

    auto model = rl::LoadModel(file_path.c_str());

    if (model.meshCount <= 0 || model.meshes == nullptr) {
      return;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
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

    std::unordered_map<unsigned int, Handle<Texture>> texture_cache;
    usize const material_count =
      static_cast<usize>(std::max(model.materialCount, 1));
    scene.materials.reserve(material_count);
    for (int i = 0; i < static_cast<int>(material_count); ++i) {
      std::string material_name;
      cgltf_material const* gltf_material = nullptr;
      if (i > 0 && static_cast<usize>(i - 1) < data->materials_count &&
          (data->materials[i - 1].name != nullptr)) {
        material_name = data->materials[i - 1].name;
      }
      if (i > 0 && static_cast<usize>(i - 1) < data->materials_count) {
        gltf_material = &data->materials[i - 1];
      }
      if (model.materials && i < model.materialCount) {
        scene.materials.push_back(
          detail::material_from_backend_material(
            model.materials[i],
            *texture_assets,
            *material_assets,
            texture_cache,
            material_name,
            gltf_material
          )
        );
      } else {
        SceneMaterial material;
        material.name = std::move(material_name);
        material.material = material_assets->add(StandardMaterial{});
        scene.materials.push_back(std::move(material));
      }
    }

    if (scene.materials.empty()) {
      SceneMaterial material;
      material.material = material_assets->add(StandardMaterial{});
      scene.materials.push_back(std::move(material));
    }

    std::vector<std::vector<ScenePrimitive>> node_primitives(data->nodes_count);
    int model_mesh_index = 0;
    for (cgltf_size node_index = 0; node_index < data->nodes_count;
         ++node_index) {
      cgltf_node& node = data->nodes[node_index];
      if (!node.mesh) {
        continue;
      }

      for (cgltf_size primitive_index = 0;
           primitive_index < node.mesh->primitives_count;
           ++primitive_index) {
        cgltf_primitive const& primitive =
          node.mesh->primitives[primitive_index];
        if (primitive.type != cgltf_primitive_type_triangles) {
          continue;
        }
        if (model_mesh_index >= model.meshCount) {
          break;
        }

        Handle<Mesh> mesh_handle =
          mesh_assets->add(model.meshes[model_mesh_index]);
        model.meshes[model_mesh_index] = gl::Mesh{};

        usize material_index = 0;
        if (model.meshMaterial && model_mesh_index < model.meshCount &&
            model.meshMaterial[model_mesh_index] >= 0) {
          material_index =
            static_cast<usize>(model.meshMaterial[model_mesh_index]);
        }
        material_index = std::min(material_index, scene.materials.size() - 1);

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
    for (int bone_index = 0; bone_index < model.skeleton.boneCount;
         ++bone_index) {
      SceneBoneData bone;
      bone.name = model.skeleton.bones[bone_index].name;
      bone.parent = model.skeleton.bones[bone_index].parent;

      if (model.skeleton.bindPose) {
        auto const& bind_pose = model.skeleton.bindPose[bone_index];
        bone.bind_pose.translation = Vec3(
          bind_pose.translation.x,
          bind_pose.translation.y,
          bind_pose.translation.z
        );
        bone.bind_pose.rotation = Quat(
          bind_pose.rotation.x,
          bind_pose.rotation.y,
          bind_pose.rotation.z,
          bind_pose.rotation.w
        );
        bone.bind_pose.scale =
          Vec3(bind_pose.scale.x, bind_pose.scale.y, bind_pose.scale.z);
      }

      bone_index_by_name.emplace(bone.name, bone_index);
      scene.bones.push_back(std::move(bone));
    }

    for (cgltf_size node_index = 0; node_index < data->nodes_count;
         ++node_index) {
      cgltf_node const& node = data->nodes[node_index];
      auto& scene_node = scene.nodes[node_index];
      scene_node.name = node.name ? node.name : "";
      scene_node.local_transform = detail::transform_from_cgltf_node(node);
      scene_node.parent =
        node.parent ? static_cast<i32>(node.parent - data->nodes) : -1;
      scene_node.primitives = std::move(node_primitives[node_index]);

      for (cgltf_size child_index = 0; child_index < node.children_count;
           ++child_index) {
        scene_node.children.push_back(
          static_cast<usize>(node.children[child_index] - data->nodes)
        );
      }

      if (!node.parent) {
        scene.roots.push_back(node_index);
      }

      if (node.name) {
        auto bone_it = bone_index_by_name.find(node.name);
        if (bone_it != bone_index_by_name.end()) {
          scene_node.bone_index = bone_it->second;
          scene.bones[bone_it->second].node_index =
            static_cast<i32>(node_index);
        }
      }
    }

    std::vector<Mat4> node_world_transforms(scene.nodes.size(), Mat4::IDENTITY);
    std::vector<bool> node_world_ready(scene.nodes.size(), false);
    std::function<Mat4(usize)> compute_node_world =
      [&](usize node_index) -> rydz_math::Mat4 {
      if (node_world_ready[node_index]) {
        return node_world_transforms[node_index];
      }

      Mat4 local = scene.nodes[node_index].local_transform.compute_matrix();
      if (scene.nodes[node_index].parent >= 0) {
        local = compute_node_world(
                  static_cast<usize>(scene.nodes[node_index].parent)
                ) *
                local;
      }

      node_world_transforms[node_index] = local;
      node_world_ready[node_index] = true;
      return local;
    };

    for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
      Mat4 node_world = compute_node_world(node_index);
      Transform primitive_local =
        detail::transform_from_matrix(node_world.inverse());

      for (auto& primitive : scene.nodes[node_index].primitives) {
        primitive.local_transform = primitive_local;
      }
    }

    scene.skins.reserve(data->skins_count);
    for (cgltf_size skin_index = 0; skin_index < data->skins_count;
         ++skin_index) {
      cgltf_skin const& skin = data->skins[skin_index];
      SceneSkin scene_skin;
      scene_skin.name = skin.name ? skin.name : "";
      scene_skin.skeleton_root_node =
        skin.skeleton ? static_cast<i32>(skin.skeleton - data->nodes) : -1;
      scene_skin.bone_indices.reserve(skin.joints_count);

      for (cgltf_size joint_index = 0; joint_index < skin.joints_count;
           ++joint_index) {
        cgltf_node const* joint = skin.joints[joint_index];
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
        scene_skin.inverse_bind_matrices.resize(
          skin.inverse_bind_matrices->count
        );
        for (cgltf_size matrix_index = 0;
             matrix_index < skin.inverse_bind_matrices->count;
             ++matrix_index) {
          std::array<float, 16> values{};
          cgltf_accessor_read_float(
            skin.inverse_bind_matrices,
            matrix_index,
            values.data(),
            values.size()
          );
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
