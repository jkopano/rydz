#pragma once
#include "mod.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/spatial/transform.hpp"
#include "rydz_ui/system_sets.hpp"

struct UiPlugin {
public:
  static void ui_init_system(ecs::World& world) {
    if (world.has_resource<rydz::ui::UiWhiteTexture>())
      return;

    auto* textures = world.get_resource<ecs::Assets<ecs::Texture>>();
    if (!textures)
      return;

    rl::Image img = rl::GenImageColor(1, 1, rlColor{255, 255, 255, 255});
    gl::Texture tex = rl::LoadTextureFromImage(img);
    rl::UnloadImage(img);

    world.insert_resource(rydz::ui::UiWhiteTexture{textures->add(std::move(tex))});
  }

  static void ui_prepare_system(ecs::World& world) {
    auto* storage = world.get_storage<rydz::ui::UiNode>();
    if (!storage)
      return;

    storage->for_each([&](ecs::Entity e, rydz::ui::UiNode const&) {
      if (!world.has_component<rydz::ui::ComputedUiNode>(e))
        world.insert_component(e, rydz::ui::ComputedUiNode{});
    });
  }

  static void ui_layout_system(ecs::World& world) {
    auto* root = world.get_resource<rydz::ui::UiRoot>();
    auto* window = world.get_resource<ecs::Window>();
    if (!root || !window)
      return;

    rydz::ui::algorithms::compute_layout(
      world,
      root->root,
      {static_cast<float>(window->width), static_cast<float>(window->height)}
    );
  }

  static void ui_post_layout_system(ecs::World& world) {
    auto* white = world.get_resource<rydz::ui::UiWhiteTexture>();
    auto* textures = world.get_resource<ecs::Assets<ecs::Texture>>();
    auto* text_cache = world.get_resource<rydz::ui::UiTextCache>();
    auto* font_cache = world.get_resource<rydz::ui::UiFontCache>();
    if (!white || !textures || !text_cache || !font_cache)
      return;

    // --- Panele ---
    auto* panels = world.get_storage<rydz::ui::Panel>();
    if (panels) {
      panels->for_each([&](ecs::Entity e, rydz::ui::Panel const& panel) {
        auto* computed = world.get_component<rydz::ui::ComputedUiNode>(e);
        if (!computed)
          return;

        if (!world.has_component<ecs::Transform>(e))
          world.insert_component(e, ecs::Transform{});
        auto* t = world.get_component<ecs::Transform>(e);
        t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);
        t->scale = math::Vec3(computed->size.x, computed->size.y, 1.0f);

        if (!world.has_component<ecs::Sprite>(e))
          world.insert_component(e, ecs::Sprite{});
        auto* sprite = world.get_component<ecs::Sprite>(e);
        sprite->handle = white->handle;
        sprite->tint = panel.background_color;
        if (auto* node = world.get_component<rydz::ui::UiNode>(e))
          sprite->layer = node->z_index;
      });
    }

    // --- Labele ---
    auto* labels = world.get_storage<rydz::ui::Label>();
    if (!labels)
      return;

    labels->for_each([&](ecs::Entity e, rydz::ui::Label const& label) {
      if (label.text.empty())
        return;

      auto* computed = world.get_component<rydz::ui::ComputedUiNode>(e);
      if (!computed)
        return;

      if (!world.has_component<ecs::Transform>(e))
        world.insert_component(e, ecs::Transform{});
      auto* t = world.get_component<ecs::Transform>(e);
      t->translation = math::Vec3(computed->pos.x, computed->pos.y, 0.0f);
      t->scale = math::Vec3(1.0f, 1.0f, 1.0f);

      if (!world.has_component<ecs::Sprite>(e))
        world.insert_component(e, ecs::Sprite{});
      auto* sprite = world.get_component<ecs::Sprite>(e);

      std::string const key = make_label_cache_key(label);
      auto it = text_cache->items.find(key);
      if (it == text_cache->items.end()) {
        rl::Font font;
        if (label.font_path.empty()) {
          font = rl::GetFontDefault();
        } else {
          auto fit = font_cache->items.find(label.font_path);
          if (fit == font_cache->items.end()) {
            rl::Font loaded = rl::LoadFont(label.font_path.c_str());
            fit = font_cache->items.emplace(label.font_path, loaded).first;
          }
          font = fit->second;
        }

        rl::Vector2 text_size =
          rl::MeasureTextEx(font, label.text.c_str(), label.font_size, label.spacing);

        rl::Image img = rl::GenImageColor(
          static_cast<int>(text_size.x),
          static_cast<int>(text_size.y),
          rlColor{0, 0, 0, 0}
        );

        rl::ImageDrawTextEx(
          &img,
          font,
          label.text.c_str(),
          {0.0f, 0.0f},
          label.font_size,
          label.spacing,
          label.color
        );

        gl::Texture raw_tex = rl::LoadTextureFromImage(img);
        rl::UnloadImage(img);
        it = text_cache->items.emplace(key, textures->add(std::move(raw_tex))).first;
      }

      sprite->handle = it->second;
      sprite->tint = rlColor{255, 255, 255, 255};
      if (auto* node = world.get_component<rydz::ui::UiNode>(e))
        sprite->layer = node->z_index;
    });
  }

  static void install(ecs::App& app) {
    auto& world = app.world();

    app.configure_set(
      ecs::ScheduleLabel::PostUpdate,
      ecs::configure(
        rydz::ui::UiSystemSet::Prepare,
        rydz::ui::UiSystemSet::Layout,
        rydz::ui::UiSystemSet::PostLayout
      )
        .chain()
    );

    if (!world.has_resource<rydz::ui::UiRoot>()) {
      ecs::Entity root = world.spawn();
      world.insert_component(root, rydz::ui::UiNode{});
      world.insert_component(root, rydz::ui::Style{});
      world.insert_resource(rydz::ui::UiRoot{root});
    }

    if (!world.has_resource<rydz::ui::UiTextCache>())
      world.insert_resource(rydz::ui::UiTextCache{});

    if (!world.has_resource<rydz::ui::UiFontCache>())
      world.insert_resource(rydz::ui::UiFontCache{});

    app.add_systems(ecs::ScheduleLabel::Startup, ui_init_system);
    app.add_systems(rydz::ui::UiSystemSet::Prepare, ui_prepare_system)
      .add_systems(rydz::ui::UiSystemSet::Layout, ui_layout_system)
      .add_systems(rydz::ui::UiSystemSet::PostLayout, ui_post_layout_system);
  }
};
