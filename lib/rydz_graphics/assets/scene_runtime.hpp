#pragma once

#include "rydz_ecs/core/hierarchy.hpp"
#include "rydz_graphics/assets/scene_graph.hpp"
#include "rydz_graphics/material/standard_material.hpp"
#include "rydz_graphics/spatial/visibility.hpp"
#include <algorithm>
#include <ranges>

namespace ecs {

struct SceneRuntimeSystems {
  static auto cleanup_orphan_scene_entities_system(World& world) -> void {
    auto* owned_storage = world.get_storage<SceneOwned>();
    if (owned_storage == nullptr) {
      return;
    }

    std::vector<Entity> to_remove;
    owned_storage->for_each([&](Entity entity, SceneOwned const& owned) -> void {
      if (!world.entities.is_alive(owned.root) ||
          !world.has_component<SceneRoot>(owned.root)) {
        to_remove.push_back(entity);
      }
    });

    std::ranges::sort(to_remove, [](Entity lhs, Entity rhs) -> bool {
      return lhs.index() > rhs.index();
    });

    auto [first, last] = std::ranges::unique(to_remove);
    to_remove.erase(first, last);

    for (Entity entity : to_remove) {
      if (world.entities.is_alive(entity)) {
        world.despawn(entity);
      }
    }
  }

  static auto sync_scene_roots_system(World& world) -> void {
    auto* scene_roots = world.get_storage<SceneRoot>();
    auto* scene_assets = world.get_resource<Assets<Scene>>();
    if ((scene_roots == nullptr) || (scene_assets == nullptr)) {
      return;
    }

    std::vector<Entity> roots;
    scene_roots->for_each([&](Entity entity, SceneRoot const&) -> void {
      roots.push_back(entity);
    });

    for (Entity root_entity : roots) {
      auto* root = world.get_component<SceneRoot>(root_entity);
      if (root == nullptr) {
        continue;
      }

      auto* instance = world.get_component<SceneInstance>(root_entity);

      if (!root->scene.is_valid()) {
        if (instance != nullptr) {
          detail::destroy_scene_instance(world, *instance);
          world.remove_component<SceneInstance>(root_entity);
        }
        continue;
      }

      Scene const* scene = scene_assets->get(root->scene);
      if (scene == nullptr) {
        if ((instance != nullptr) && instance->scene != root->scene) {
          detail::destroy_scene_instance(world, *instance);
          world.remove_component<SceneInstance>(root_entity);
        }
        continue;
      }

      bool rebuild = (instance == nullptr) || (instance->scene != root->scene) ||
                     (!detail::scene_instance_alive(world, *instance)) ||
                     (!detail::scene_instance_matches_shape(*scene, *instance));

      if (rebuild) {
        if (instance != nullptr) {
          detail::destroy_scene_instance(world, *instance);
        }
        world.insert_component(
          root_entity,
          detail::build_scene_instance(world, root_entity, *scene, root->scene)
        );
        continue;
      }

      detail::sync_scene_instance(world, root_entity, *scene, *instance);
    }
  }

private:
  struct detail {
    static auto scene_instance_alive(World const& world, SceneInstance const& inst)
      -> bool {
      return std::ranges::all_of(inst.owned_entities, [&](Entity entity) -> bool {
        return world.entities.is_alive(entity);
      });
    }

    static auto ensure_transform(World& world, Entity entity, Transform const& transform)
      -> void {
      if (auto* existing = world.get_component<Transform>(entity)) {
        *existing = transform;
      } else {
        world.insert_component(entity, transform);
      }
    }

    static auto ensure_parent(World& world, Entity entity, Entity parent_entity) -> void {
      if (auto* existing = world.get_component<Parent>(entity)) {
        existing->entity = parent_entity;
      } else {
        world.insert_component(entity, Parent{parent_entity});
      }
    }

    static auto ensure_visibility(World& world, Entity entity) -> void {
      if (!world.has_component<Visibility>(entity)) {
        world.insert_component(entity, Visibility::Inherited);
      } else if (auto* visibility = world.get_component<Visibility>(entity)) {
        *visibility = Visibility::Inherited;
      }
    }

    static auto ensure_scene_owned(World& world, Entity entity, Entity root) -> void {
      if (auto* owned = world.get_component<SceneOwned>(entity)) {
        owned->root = root;
      } else {
        world.insert_component(entity, SceneOwned{root});
      }
    }

    static auto ensure_node_instance(
      World& world, Entity entity, Entity root, usize node_index, i32 bone_index
    ) -> void {
      if (auto* node = world.get_component<SceneNodeInstance>(entity)) {
        node->root = root;
        node->node_index = node_index;
      } else {
        world.insert_component(
          entity, SceneNodeInstance{.root = root, .node_index = node_index}
        );
      }

      if (bone_index >= 0) {
        if (auto* joint = world.get_component<SceneJoint>(entity)) {
          joint->bone_index = static_cast<usize>(bone_index);
        } else {
          world.insert_component(entity, SceneJoint{static_cast<usize>(bone_index)});
        }
      } else {
        world.remove_component<SceneJoint>(entity);
      }
    }

    static auto ensure_primitive_instance(
      World& world,
      Entity entity,
      Entity root,
      usize node_index,
      usize primitive_index,
      i32 skin_index
    ) -> void {
      if (auto* primitive = world.get_component<ScenePrimitiveInstance>(entity)) {
        primitive->root = root;
        primitive->node_index = node_index;
        primitive->primitive_index = primitive_index;
      } else {
        world.insert_component(
          entity,
          ScenePrimitiveInstance{
            .root = root, .node_index = node_index, .primitive_index = primitive_index
          }
        );
      }

      if (skin_index >= 0) {
        if (auto* binding = world.get_component<SceneSkinBinding>(entity)) {
          binding->skin_index = skin_index;
        } else {
          world.insert_component(entity, SceneSkinBinding{skin_index});
        }
      } else {
        world.remove_component<SceneSkinBinding>(entity);
      }
    }

    static auto ensure_mesh(World& world, Entity entity, Handle<Mesh> mesh) -> void {
      if (auto* existing = world.get_component<Mesh3d>(entity)) {
        existing->mesh = mesh;
      } else {
        world.insert_component(entity, Mesh3d{mesh});
      }
    }

    static auto ensure_material(World& world, Entity entity, Handle<Material> material)
      -> void {
      if (auto* existing = world.get_component<MeshMaterial3d<>>(entity)) {
        existing->material = material;
      } else {
        world.insert_component(entity, MeshMaterial3d{material});
      }
    }

    static auto destroy_scene_instance(World& world, SceneInstance const& inst) -> void {
      for (auto owned_entitie : std::ranges::reverse_view(inst.owned_entities)) {
        if (world.entities.is_alive(owned_entitie)) {
          world.despawn(owned_entitie);
        }
      }
    }

    static auto build_scene_instance(
      World& world, Entity root_entity, Scene const& scene, Handle<Scene> scene_handle
    ) -> SceneInstance {
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
        auto const& node = scene.nodes[node_index];
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
        auto const& node = scene.nodes[node_index];
        auto& primitive_entities = instance.primitive_entities[node_index];
        primitive_entities.resize(node.primitives.size());

        for (usize primitive_index = 0; primitive_index < node.primitives.size();
             ++primitive_index) {
          auto const& primitive = node.primitives[primitive_index];
          Entity entity = world.spawn();
          primitive_entities[primitive_index] = entity;
          instance.owned_entities.push_back(entity);

          auto const material_index =
            std::min(primitive.material_index, scene.materials.size() - 1);

          ensure_scene_owned(world, entity, root_entity);
          ensure_primitive_instance(
            world, entity, root_entity, node_index, primitive_index, primitive.skin_index
          );
          ensure_visibility(world, entity);
          ensure_transform(world, entity, primitive.local_transform);
          ensure_parent(world, entity, instance.node_entities[node_index]);
          ensure_mesh(world, entity, primitive.mesh);
          ensure_material(world, entity, scene.materials[material_index].material);
        }
      }

      return instance;
    }

    static auto scene_instance_matches_shape(
      Scene const& scene, SceneInstance const& instance
    ) -> bool {
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

    static auto sync_scene_instance(
      World& world, Entity root_entity, Scene const& scene, SceneInstance& instance
    ) -> void {
      for (usize node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        auto const& node = scene.nodes[node_index];
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
        auto const& node = scene.nodes[node_index];

        for (usize primitive_index = 0; primitive_index < node.primitives.size();
             ++primitive_index) {
          auto const& primitive = node.primitives[primitive_index];
          Entity entity = instance.primitive_entities[node_index][primitive_index];
          auto const material_index =
            std::min(primitive.material_index, scene.materials.size() - 1);

          ensure_scene_owned(world, entity, root_entity);
          ensure_primitive_instance(
            world, entity, root_entity, node_index, primitive_index, primitive.skin_index
          );
          ensure_visibility(world, entity);
          ensure_transform(world, entity, primitive.local_transform);
          ensure_parent(world, entity, instance.node_entities[node_index]);
          ensure_mesh(world, entity, primitive.mesh);
          ensure_material(world, entity, scene.materials[material_index].material);
        }
      }
    }
  };
};

} // namespace ecs
