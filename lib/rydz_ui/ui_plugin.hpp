#pragma once
#include "rydz_ecs/app.hpp"
#include "rydz_graphics/render_extract.hpp"
#include "rydz_graphics/transform.hpp"
#include "rydz_ui.hpp"
#include "rydz_ui/system_sets.hpp"

struct UiPlugin {
public:
  static void ui_init_system(ecs::World &world) {
    if (world.has_resource<rydz::ui::UiWhiteTexture>()) {
      return;
    }

    auto *textures = world.get_resource<ecs::Assets<rl::Texture2D>>();
    if (!textures) {
      return;
    }

    rl::Image img = rl::GenImageColor(1, 1, rl::Color{255, 255, 255, 255});
    rl::Texture2D tex = rl::LoadTextureFromImage(img);
    rl::UnloadImage(img);

    auto handle = textures->add(tex);
    world.insert_resource(rydz::ui::UiWhiteTexture{handle});
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
    auto *panels = world.get_storage<rydz::ui::Panel>();
    auto *white = world.get_resource<rydz::ui::UiWhiteTexture>();
    auto *textures = world.get_resource<ecs::Assets<rl::Texture2D>>();
    auto *text_cache = world.get_resource<rydz::ui::UiTextCache>();

    if (panels) {
      panels->for_each([&](ecs::Entity e, const rydz::ui::Panel &panel) {
        auto *computed = world.get_component<rydz::ui::ComputedUiNode>(e);
        if (!computed)
          return;

        if (!world.has_component<ecs::Transform>(e)) {
          world.insert_component(e, ecs::Transform{});
        }
        auto *t = world.get_component<ecs::Transform>(e);
        t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);
        t->scale = math::Vec3(computed->size.x, computed->size.y, 1.0f);

        if (!world.has_component<ecs::Texture>(e)) {
          world.insert_component(e, ecs::Texture{});
        }
        auto *tex = world.get_component<ecs::Texture>(e);
        if (white) {
          tex->handle = white->handle;
        }
        tex->tint = panel.background_color;
        if (auto *node = world.get_component<rydz::ui::UiNode>(e)) {
          tex->layer = node->z_index;
        }
      });
    }

    auto *labels = world.get_storage<rydz::ui::Label>();
    if (!labels || !textures || !text_cache) {
      return;
    }

    labels->for_each([&](ecs::Entity e, const rydz::ui::Label &label) {
      auto *computed = world.get_component<rydz::ui::ComputedUiNode>(e);
      if (!computed)
        return;

      if (!world.has_component<ecs::Transform>(e)) {
        world.insert_component(e, ecs::Transform{});
      }

      if (!world.has_component<ecs::Texture>(e)) {
        world.insert_component(e, ecs::Texture{});
      }

      auto *tex = world.get_component<ecs::Texture>(e);

      const std::string key = make_label_cache_key(label);
      auto it = text_cache->items.find(key);
      if (it == text_cache->items.end()) {
        rl::Image img =
            rl::ImageText(label.text.c_str(),
                          static_cast<int>(label.font_size), label.color);
        rl::Texture2D timg = rl::LoadTextureFromImage(img);
        rl::UnloadImage(img);
        auto handle = textures->add(timg);
        it = text_cache->items.emplace(key, handle).first;
      }

      tex->handle = it->second;
      tex->tint = rl::Color{255, 255, 255, 255};
      if (auto *node = world.get_component<rydz::ui::UiNode>(e)) {
        tex->layer = node->z_index;
      }

      auto *t = world.get_component<ecs::Transform>(e);
      t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);

      rl::Texture2D *tex_asset = textures->get(tex->handle);
      if (tex_asset && tex_asset->width > 0 && tex_asset->height > 0) {
        t->scale = math::Vec3(computed->size.x / tex_asset->width,
                              computed->size.y / tex_asset->height, 1.0f);
      } else {
        t->scale = math::Vec3(1.0f, 1.0f, 1.0f);
      }
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
