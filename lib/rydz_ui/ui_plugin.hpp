#pragma once
#include "mod.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_graphics/render_extract.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_ui/system_sets.hpp"

struct UiPlugin {
public:
  static void ui_init_system(ecs::World &world) {
    if (world.has_resource<rydz::ui::UiQuadMesh>())
      return;

    auto *materials = world.get_resource<ecs::Assets<ecs::Material>>();
    auto *meshes = world.get_resource<ecs::Assets<ecs::Mesh>>();
    if (!materials || !meshes)
      return;

    auto quad_handle = meshes->add(ecs::mesh::plane(1.0f, 1.0f));
    world.insert_resource(rydz::ui::UiQuadMesh{quad_handle});
  }

  static void ui_prepare_system(ecs::World &world) {
    auto *storage = world.get_storage<rydz::ui::UiNode>();
    if (!storage)
      return;

    storage->for_each([&](ecs::Entity e, const rydz::ui::UiNode &) {
      if (!world.has_component<rydz::ui::ComputedUiNode>(e)) {
        world.insert_component(e, rydz::ui::ComputedUiNode{});
      }
    });
  }

  static void ui_layout_system(ecs::World &world) {
    auto *root = world.get_resource<rydz::ui::UiRoot>();
    auto *window = world.get_resource<ecs::Window>();
    if (!root || !window)
      return;

    math::Vec2 root_size{
        static_cast<float>(window->width),
        static_cast<float>(window->height),
    };

    rydz::ui::algorithms::compute_layout(world, root->root, root_size);
  }

  static void ui_post_layout_system(ecs::World &world) {
    // auto *white = world.get_resource<rydz::ui::UiWhiteTexture>();
    auto *quad_res = world.get_resource<rydz::ui::UiQuadMesh>();
    auto *materials = world.get_resource<ecs::Assets<ecs::Material>>();
    auto *textures = world.get_resource<ecs::Assets<ecs::Texture>>();
    auto *text_cache = world.get_resource<rydz::ui::UiTextCache>();

    if (!quad_res || !materials)
      return;

    auto quad_handle = quad_res->mesh;

    auto *panels = world.get_storage<rydz::ui::Panel>();
    if (panels) {
      panels->for_each([&](ecs::Entity e, const rydz::ui::Panel &panel) {
        auto *computed = world.get_component<rydz::ui::ComputedUiNode>(e);
        if (!computed)
          return;

        if (!world.has_component<ecs::Transform>(e))
          world.insert_component(e, ecs::Transform{});
        auto *t = world.get_component<ecs::Transform>(e);
        t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);
        t->scale = math::Vec3(computed->size.x, computed->size.y, 1.0f);

        if (!world.has_component<ecs::Mesh3d>(e))
          world.insert_component(e, ecs::Mesh3d{quad_handle});
        else
          world.get_component<ecs::Mesh3d>(e)->mesh = quad_handle;

        if (!world.has_component<ecs::MeshMaterial3d>(e))
          world.insert_component(e, ecs::MeshMaterial3d{});
        auto *mat_comp = world.get_component<ecs::MeshMaterial3d>(e);

        auto color_mat = materials->add(
            ecs::StandardMaterial::from_color(panel.background_color));
        mat_comp->material = color_mat;
      });
    }

    auto *labels = world.get_storage<rydz::ui::Label>();
    if (!labels || !textures || !text_cache)
      return;

    labels->for_each([&](ecs::Entity e, const rydz::ui::Label &label) {
      auto *computed = world.get_component<rydz::ui::ComputedUiNode>(e);
      if (!computed)
        return;

      if (!world.has_component<ecs::Transform>(e))
        world.insert_component(e, ecs::Transform{});
      auto *t = world.get_component<ecs::Transform>(e);
      t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);
      t->scale = math::Vec3(computed->size.x, computed->size.y, 1.0f);

      if (!world.has_component<ecs::Mesh3d>(e))
        world.insert_component(e, ecs::Mesh3d{quad_handle});

      if (!world.has_component<ecs::MeshMaterial3d>(e))
        world.insert_component(e, ecs::MeshMaterial3d{});
      auto *mat_comp = world.get_component<ecs::MeshMaterial3d>(e);

      const std::string key = make_label_cache_key(label);
      auto it = text_cache->items.find(key);
      if (it == text_cache->items.end()) {
        rl::Image img = rl::ImageText(
            label.text.c_str(), static_cast<int>(label.font_size), label.color);
        gl::Texture raw_tex = rl::LoadTextureFromImage(img);
        rl::UnloadImage(img);

        auto tex_handle = textures->add(raw_tex);
        it = text_cache->items.emplace(key, tex_handle).first;
      }

      auto label_mat = materials->add(
          ecs::StandardMaterial::from_texture(it->second, label.color));
      mat_comp->material = label_mat;
    });
  }

  static void install(ecs::App &app) {
    auto &world = app.world();

    app.configure_set(ecs::ScheduleLabel::PostUpdate,
                      ecs::configure(rydz::ui::UiSystemSet::Prepare,
                                     rydz::ui::UiSystemSet::Layout,
                                     rydz::ui::UiSystemSet::PostLayout)
                          .chain());

    if (!world.has_resource<rydz::ui::UiRoot>()) {
      ecs::Entity root = world.spawn();
      world.insert_component(root, rydz::ui::UiNode{});
      world.insert_component(root, rydz::ui::Style{});
      world.insert_resource(rydz::ui::UiRoot{root});
    }

    if (!world.has_resource<rydz::ui::UiTextCache>()) {
      world.insert_resource(rydz::ui::UiTextCache{});
    }

    app.add_systems(ecs::ScheduleLabel::Startup, ui_init_system);
    app.add_systems(rydz::ui::UiSystemSet::Prepare, ui_prepare_system)
        .add_systems(rydz::ui::UiSystemSet::Layout, ui_layout_system)
        .add_systems(rydz::ui::UiSystemSet::PostLayout, ui_post_layout_system);
  }
};
