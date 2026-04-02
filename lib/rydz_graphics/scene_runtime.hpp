#pragma once
#include "scene_graph.hpp"
#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_graphics/visibility.hpp"
#include <algorithm>

namespace ecs {

namespace detail {

inline bool scene_instance_alive(const World &world, const SceneInstance &inst) {
  for (Entity entity : inst.owned_entities) {
    if (!world.entities.is_alive(entity)) {
      return false;
    }
  }
  return true;
}

inline void ensure_transform(World &world, Entity entity,
                             const Transform &transform) {
  if (auto *existing = world.get_component<Transform>(entity)) {
    *existing = transform;
  } else {
    world.insert_component(entity, transform);
  }
}

inline void ensure_parent(World &world, Entity entity, Entity parent_entity) {
  if (auto *existing = world.get_component<Parent>(entity)) {
    existing->entity = parent_entity;
  } else {
    world.insert_component(entity, Parent{parent_entity});
  }
}

inline void ensure_visibility(World &world, Entity entity) {
  if (!world.has_component<Visibility>(entity)) {
    world.insert_component(entity, Visibility::Inherited);
  } else if (auto *visibility = world.get_component<Visibility>(entity)) {
    *visibility = Visibility::Inherited;
  }
}

inline void ensure_scene_owned(World &world, Entity entity, Entity root) {
  if (auto *owned = world.get_component<SceneOwned>(entity)) {
    owned->root = root;
  } else {
    world.insert_component(entity, SceneOwned{root});
  }
}

inline void ensure_node_instance(World &world, Entity entity, Entity root,
                                 usize node_index, i32 bone_index) {
  if (auto *node = world.get_component<SceneNodeInstance>(entity)) {
    node->root = root;
    node->node_index = node_index;
  } else {
    world.insert_component(entity, SceneNodeInstance{root, node_index});
  }

  if (bone_index >= 0) {
    if (auto *joint = world.get_component<SceneJoint>(entity)) {
      joint->bone_index = static_cast<usize>(bone_index);
    } else {
      world.insert_component(entity,
                             SceneJoint{static_cast<usize>(bone_index)});
    }
  } else {
    world.remove_component<SceneJoint>(entity);
  }
}

inline void ensure_primitive_instance(World &world, Entity entity, Entity root,
                                      usize node_index, usize primitive_index,
                                      i32 skin_index) {
  if (auto *primitive = world.get_component<ScenePrimitiveInstance>(entity)) {
    primitive->root = root;
    primitive->node_index = node_index;
    primitive->primitive_index = primitive_index;
  } else {
    world.insert_component(
        entity, ScenePrimitiveInstance{root, node_index, primitive_index});
  }

  if (skin_index >= 0) {
    if (auto *binding = world.get_component<SceneSkinBinding>(entity)) {
      binding->skin_index = skin_index;
    } else {
      world.insert_component(entity, SceneSkinBinding{skin_index});
    }
  } else {
    world.remove_component<SceneSkinBinding>(entity);
  }
}

inline void ensure_mesh(World &world, Entity entity, Handle<rl::Mesh> mesh) {
  if (auto *existing = world.get_component<Mesh3d>(entity)) {
    existing->mesh = mesh;
  } else {
    world.insert_component(entity, Mesh3d{mesh});
  }
}

inline void ensure_material(World &world, Entity entity,
                            const StandardMaterial &material) {
  if (auto *existing = world.get_component<Material3d>(entity)) {
    existing->material = material;
  } else {
    world.insert_component(entity, Material3d{material});
  }
}

inline void destroy_scene_instance(World &world, const SceneInstance &inst) {
  for (auto it = inst.owned_entities.rbegin(); it != inst.owned_entities.rend();
       ++it) {
    if (world.entities.is_alive(*it)) {
      world.despawn(*it);
    }
  }
}

inline SceneInstance build_scene_instance(World &world, Entity root_entity,
                                          const Scene &scene,
                                          Handle<Scene> scene_handle) {
  SceneInstance instance;
  instance.scene = scene_handle;
  instance.node_entities.resize(scene.nodes.size());
  instance.primitive_entities.resize(scene.nodes.size());

  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    Entity entity = world.spawn();
    instance.node_entities[node_index] = entity;
    instance.owned_entities.push_back(entity);
  }

  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    const auto &node = scene.nodes[node_index];
    Entity entity = instance.node_entities[node_index];
    Entity parent_entity =
        node.parent >= 0 ? instance.node_entities[node.parent] : root_entity;

    ensure_scene_owned(world, entity, root_entity);
    ensure_node_instance(world, entity, root_entity, node_index, node.bone_index);
    ensure_visibility(world, entity);
    ensure_transform(world, entity, node.local_transform);
    ensure_parent(world, entity, parent_entity);
  }

  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    const auto &node = scene.nodes[node_index];
    auto &primitive_entities = instance.primitive_entities[node_index];
    primitive_entities.resize(node.primitives.size());

    for (usize primitive_index = 0; primitive_index < node.primitives.size();
         ++primitive_index) {
      const auto &primitive = node.primitives[primitive_index];
      Entity entity = world.spawn();
      primitive_entities[primitive_index] = entity;
      instance.owned_entities.push_back(entity);

      const auto material_index =
          std::min(primitive.material_index, scene.materials.size() - 1);

      ensure_scene_owned(world, entity, root_entity);
      ensure_primitive_instance(world, entity, root_entity, node_index,
                                primitive_index, primitive.skin_index);
      ensure_visibility(world, entity);
      ensure_transform(world, entity, primitive.local_transform);
      ensure_parent(world, entity, instance.node_entities[node_index]);
      ensure_mesh(world, entity, primitive.mesh);
      ensure_material(world, entity, scene.materials[material_index].material);
    }
  }

  return instance;
}

inline bool scene_instance_matches_shape(const Scene &scene,
                                         const SceneInstance &instance) {
  if (instance.node_entities.size() != scene.nodes.size()) {
    return false;
  }
  if (instance.primitive_entities.size() != scene.nodes.size()) {
    return false;
  }

  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    if (instance.primitive_entities[node_index].size() !=
        scene.nodes[node_index].primitives.size()) {
      return false;
    }
  }
  return true;
}

inline void sync_scene_instance(World &world, Entity root_entity,
                                const Scene &scene, SceneInstance &instance) {
  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    const auto &node = scene.nodes[node_index];
    Entity entity = instance.node_entities[node_index];
    Entity parent_entity =
        node.parent >= 0 ? instance.node_entities[node.parent] : root_entity;

    ensure_scene_owned(world, entity, root_entity);
    ensure_node_instance(world, entity, root_entity, node_index, node.bone_index);
    ensure_visibility(world, entity);
    ensure_transform(world, entity, node.local_transform);
    ensure_parent(world, entity, parent_entity);
  }

  for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
    const auto &node = scene.nodes[node_index];

    for (usize primitive_index = 0; primitive_index < node.primitives.size();
         ++primitive_index) {
      const auto &primitive = node.primitives[primitive_index];
      Entity entity = instance.primitive_entities[node_index][primitive_index];
      const auto material_index =
          std::min(primitive.material_index, scene.materials.size() - 1);

      ensure_scene_owned(world, entity, root_entity);
      ensure_primitive_instance(world, entity, root_entity, node_index,
                                primitive_index, primitive.skin_index);
      ensure_visibility(world, entity);
      ensure_transform(world, entity, primitive.local_transform);
      ensure_parent(world, entity, instance.node_entities[node_index]);
      ensure_mesh(world, entity, primitive.mesh);
      ensure_material(world, entity, scene.materials[material_index].material);
    }
  }
}

} // namespace detail

inline void cleanup_orphan_scene_entities_system(World &world) {
  auto *owned_storage = world.get_storage<SceneOwned>();
  if (!owned_storage) {
    return;
  }

  std::vector<Entity> to_remove;
  owned_storage->for_each([&](Entity entity, const SceneOwned &owned) {
    if (!world.entities.is_alive(owned.root) ||
        !world.has_component<SceneRoot>(owned.root)) {
      to_remove.push_back(entity);
    }
  });

  std::sort(to_remove.begin(), to_remove.end(),
            [](Entity lhs, Entity rhs) { return lhs.index() > rhs.index(); });
  to_remove.erase(std::unique(to_remove.begin(), to_remove.end()),
                  to_remove.end());

  for (Entity entity : to_remove) {
    if (world.entities.is_alive(entity)) {
      world.despawn(entity);
    }
  }
}

inline void sync_scene_roots_system(World &world) {
  auto *scene_roots = world.get_storage<SceneRoot>();
  auto *scene_assets = world.get_resource<Assets<Scene>>();
  if (!scene_roots || !scene_assets) {
    return;
  }

  std::vector<Entity> roots;
  scene_roots->for_each(
      [&](Entity entity, const SceneRoot &) { roots.push_back(entity); });

  for (Entity root_entity : roots) {
    auto *root = world.get_component<SceneRoot>(root_entity);
    if (!root) {
      continue;
    }

    auto *instance = world.get_component<SceneInstance>(root_entity);

    if (!root->scene.is_valid()) {
      if (instance) {
        detail::destroy_scene_instance(world, *instance);
        world.remove_component<SceneInstance>(root_entity);
      }
      continue;
    }

    const Scene *scene = scene_assets->get(root->scene);
    if (!scene) {
      if (instance && instance->scene != root->scene) {
        detail::destroy_scene_instance(world, *instance);
        world.remove_component<SceneInstance>(root_entity);
      }
      continue;
    }

    bool rebuild = false;
    if (!instance) {
      rebuild = true;
    } else if (instance->scene != root->scene) {
      rebuild = true;
    } else if (!detail::scene_instance_alive(world, *instance)) {
      rebuild = true;
    } else if (!detail::scene_instance_matches_shape(*scene, *instance)) {
      rebuild = true;
    }

    if (rebuild) {
      if (instance) {
        detail::destroy_scene_instance(world, *instance);
      }
      world.insert_component(root_entity,
                             detail::build_scene_instance(world, root_entity,
                                                          *scene, root->scene));
      continue;
    }

    detail::sync_scene_instance(world, root_entity, *scene, *instance);
  }
}

} // namespace ecs
