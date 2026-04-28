#pragma once
#include "mod.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/spatial/transform.hpp"

struct UiPlugin {
public:
  static auto ui_input_bridge(
    rydz::ui::UiPointerState pointer, ecs::Res<ecs::Input> input
  ) -> void {
    pointer.set_cursor(input->mouse_position());
    pointer.set_primary_down(input->mouse_down(0));
    pointer.set_primary_up(input->mouse_down(0));
  }

  static auto ui_node_contains_cursor(
    rydz::ui::ComputedUiNode const& node, math::Vec2 cursor
  ) -> bool {
    return cursor.x >= node.pos.x && cursor.x <= (node.pos.x + node.size.x) &&
           cursor.y >= node.pos.y && cursor.y <= (node.pos.y + node.size.y);
  }

  static auto hit_test_ui(ecs::World& world, math::Vec2 cursor)
    -> std::optional<ecs::Entity> {
    auto* nodes = world.get_storage<rydz::ui::UiNode>();
    if (!nodes) {
      return std::nullopt;
    }

    auto best = std::optional<ecs::Entity>{};
    auto best_z = std::numeric_limits<int>::min();

    nodes->for_each([&](ecs::Entity e, rydz::ui::UiNode const& node) {
      auto* computed = world.get_component<rydz::ui::ComputedUiNode>(e);
      if (!computed) {
        return;
      }

      if (
        auto* style = world.get_component<rydz::ui::Style>(e);
        style != nullptr && style->display == rydz::ui::Display::None
      ) {
        return;
      }

      if (computed->size.x <= 0.0f || computed->size.y <= 0.0f) {
        return;
      }

      if (!ui_node_contains_cursor(*computed, cursor)) {
        return;
      }

      if (node.z_index >= best_z) {
        best_z = node.z_index;
        best = e;
      }
    });

    return best;
  }

  static void update_hover_state(
    ecs::World& world,
    rydz::ui::UiPointerState const& pointer,
    rydz::ui::UiInteractionState& state,
    std::optional<ecs::Entity> next_hovered
  ) {
    if (state.hovered_entity == next_hovered) {
      return;
    }

    if (state.hovered_entity) {
      world.trigger(
        rydz::ui::UiHoverLeave{
          .target = *state.hovered_entity,
          .cursor = pointer.cursor,
        }
      );
    }

    if (next_hovered) {
      world.trigger(
        rydz::ui::UiHoverEnter{
          .target = *next_hovered,
          .cursor = pointer.cursor,
        }
      );
    }

    state.hovered_entity = next_hovered;
  }

  static void update_focus_state(
    ecs::World& world,
    rydz::ui::UiPointerState const& pointer,
    rydz::ui::UiInteractionState& state,
    std::optional<ecs::Entity> next_focused
  ) {
    if (state.focused_entity == next_focused) {
      return;
    }

    if (state.focused_entity) {
      world.trigger(rydz::ui::UiFocusLost{.target = *state.focused_entity});
    }

    if (next_focused) {
      world.trigger(rydz::ui::UiFocusGained{.target = *next_focused});
    }

    state.focused_entity = next_focused;
  }

  static void ui_interaction_system(ecs::World& world) {
    auto* pointer = world.get_resource<rydz::ui::UiPointerState>();
    auto* state = world.get_resource<rydz::ui::UiInteractionState>();
    if (!pointer || !state) {
      return;
    }

    auto next_hovered = hit_test_ui(world, pointer->cursor);
    update_hover_state(world, *pointer, *state, next_hovered);

    if (pointer->primary_pressed && next_hovered) {
      state->pressed_entity = next_hovered;
      world.trigger(
        rydz::ui::UiClick{
          .target = *next_hovered,
          .cursor = pointer->cursor,
          .button = pointer->primary_button,
        }
      );
      update_focus_state(world, *pointer, *state, next_hovered);
    }

    if (pointer->primary_released) {
      state->pressed_entity.reset();
    }
  }

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
        rydz::ui::UiSystemSet::Interaction,
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

    if (!world.has_resource<rydz::ui::UiPointerState>())
      world.insert_resource(rydz::ui::UiPointerState{});

    if (!world.has_resource<rydz::ui::UiInteractionState>())
      world.insert_resource(rydz::ui::UiInteractionState{});

    app.add_event<rydz::ui::UiHoverEnter>()
      .add_event<rydz::ui::UiHoverLeave>()
      .add_event<rydz::ui::UiClick>()
      .add_event<rydz::ui::UiFocusGained>()
      .add_event<rydz::ui::UiFocusLost>();

    app.add_systems(ecs::ScheduleLabel::Startup, ui_init_system);
    app.add_systems(rydz::ui::UiSystemSet::Prepare, ui_prepare_system)
      .add_systems(rydz::ui::UiSystemSet::Layout, ui_layout_system)
      .add_systems(rydz::ui::UiSystemSet::Interaction, ui_interaction_system)
      .add_systems(rydz::ui::UiSystemSet::PostLayout, ui_post_layout_system);
  }
};
