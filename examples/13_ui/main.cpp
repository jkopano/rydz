#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_platform/mod.hpp"
#include "rydz_ui/mod.hpp"
#include "rydz_ui/ui_plugin.hpp"

using namespace ecs;
using namespace math;

struct UiMarker {};
struct WasdKeyMarker {
  int keycode;
};

void setup(Res<rydz::ui::UiRoot> root, Cmd cmd) {
  if (!root.ptr)
    return;

  cmd.entity(root->root)
      .insert(rydz::ui::Style{
          .direction = rydz::ui::Direction::Row,
          .align = rydz::ui::Align::Start,
          .justify = rydz::ui::Justify::End,
          .padding =
              rydz::ui::UiRect{
                  .left = 10, .top = 10, .right = 10, .bottom = 10},
      });

  Entity info_panel =
      cmd.spawn(
             rydz::ui::UiNode{},
             rydz::ui::Panel{rl::Color{.r = 200, .g = 60, .b = 60, .a = 128}},
             rydz::ui::Style{
                 .direction = rydz::ui::Direction::Column,
                 .padding =
                     rydz::ui::UiRect{
                         .left = 10, .top = 10, .right = 10, .bottom = 10},
                 .margin =
                     rydz::ui::UiRect{
                         .left = 0, .top = 0, .right = 10, .bottom = 0},
                 .size = {.width = rydz::ui::SizeValue::px(200.0f),
                          .height = rydz::ui::SizeValue::px(80.0f)},
             },
             Parent{root->root})
          .id();

  cmd.spawn(rydz::ui::UiNode{},
            rydz::ui::Label{.text = "Player Status", .font_size = 14.0f},
            rydz::ui::Style{}, Parent{info_panel});
  cmd.spawn(rydz::ui::UiNode{}, rydz::ui::Label{.text = "", .font_size = 18.0f},
            rydz::ui::Style{}, Parent{info_panel}, UiMarker{});

  Entity wasd_container =
      cmd.spawn(rydz::ui::UiNode{}, rydz::ui::Panel{rl::Color{0, 0, 0, 160}},
                rydz::ui::Style{
                    .direction = rydz::ui::Direction::Row,
                    .align = rydz::ui::Align::Center,
                    .justify = rydz::ui::Justify::Center,
                    .padding =
                        rydz::ui::UiRect{
                            .left = 5, .top = 5, .right = 5, .bottom = 5},
                    .size = {.width = rydz::ui::SizeValue::px(210.0f),
                             .height = rydz::ui::SizeValue::px(60.0F)},
                },
                Parent{root->root})
          .id();

  auto spawn_key = [&](int keycode, const std::string &label_text) {
    Entity key_box =
        cmd.spawn(
               rydz::ui::UiNode{},
               rydz::ui::Panel{rl::Color{.r = 80, .g = 80, .b = 80, .a = 255}},
               rydz::ui::Style{
                   .direction = rydz::ui::Direction::Row,
                   .align = rydz::ui::Align::Center,
                   .justify = rydz::ui::Justify::Center,
                   .margin =
                       rydz::ui::UiRect{
                           .left = 4, .top = 4, .right = 4, .bottom = 4},
                   .size = {.width = rydz::ui::SizeValue::px(40.0f),
                            .height = rydz::ui::SizeValue::px(40.0f)},
               },
               Parent{wasd_container}, WasdKeyMarker{keycode})
            .id();

    cmd.spawn(rydz::ui::UiNode{},
              rydz::ui::Label{
                  .text = label_text,
                  .font_size = 20.0f,
                  .color = rl::Color{.r = 255, .g = 255, .b = 255, .a = 255}},
              rydz::ui::Style{}, Parent{key_box});
  };

  spawn_key(KEY_W, "W");
  spawn_key(KEY_A, "A");
  spawn_key(KEY_S, "S");
  spawn_key(KEY_D, "D");
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
                     .window = {.width = 800,
                                .height = 600,
                                .title = "13 - UI",
                                .target_fps = 60},
                 }))
      .add_plugin(RenderPlugin::install)
      .add_plugin(Input::install)
      .add_plugin(UiPlugin::install)
      .add_systems(Startup, setup)
      .run();
}
