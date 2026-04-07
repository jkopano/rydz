// 01 - Hello Window
// Pokazuje: App, Window, pluginy, ClearColor, prosty system Update

#include "rl.hpp"
#include "rydz_ecs/rydz_ecs.hpp"
#include "rydz_graphics/render_plugin.hpp"
#include "rydz_ui/ui_plugin.hpp"
#include "rydz_ui/rydz_ui.hpp"
#include <print>

using namespace ecs;
namespace ui = rydz::ui;

void setup(Cmd cmd) {
  cmd.spawn(Camera3DComponent::perspective(), ActiveCamera{},
            ClearColor{{40, 80, 120, 255}}, Transform{});
}

void hello_system(Res<Time> time) {
  if (static_cast<int>(time->elapsed_seconds) % 2 == 0 &&
      time->frame_count % 120 == 1) {
    std::println("elapsed: {:.1f}s  frame: {}", time->elapsed_seconds,
                 time->frame_count);
  }
}

void setup_ui(Res<ui::UiRoot> root, Cmd cmd) {
  if (!root.ptr) {
    return;
  }

  cmd.entity(root->root)
      .insert(ui::Style{
          .direction = ui::Direction::Row,
          .align = ui::Align::Start,
          .justify = ui::Justify::End,
          .padding = ui::UiRect{10, 10, 10, 10},
      });

  Entity panel = cmd
                     .spawn(ui::UiNode{},
                            ui::Panel{rl::Color{200, 60, 60, 255}},
                            ui::Style{
                                .direction = ui::Direction::Column,
                                .padding = ui::UiRect{10, 10, 10, 10},
                                .size = {ui::SizeValue::px(300.0f),
                                         ui::SizeValue::px(120.0f)},
                            },
                            Parent{root->root})
                     .id();

  cmd.spawn(ui::UiNode{},
            ui::Label{.text = "Hello UI", .font_size = 18.0f},
            ui::Style{}, Parent{panel});
}

int main() {
  // Tworzymy App i dodajemy pluginy - pluginy to tylko jakiś sposób/konwencja
  // na grupowanie systemów/logiki etc
  App app;
  // defaultowe pluginy z ecs na razie to:
  // - time_plugin
  // - window_plugin (jest o tyle różny że to funkcja zwracająca funkcje)
  // - input_plugin
  // - render_plugin
  app.add_plugin(Window::install({800, 600, "01 - Hello Window", 60}))
      .add_plugin(time_plugin)
      .add_plugin(RenderPlugin::install)
      .add_plugin(UiPlugin::install)
      .add_systems(ScheduleLabel::Update, hello_system)
      .add_systems(ScheduleLabel::Startup, setup)
      .add_systems(ScheduleLabel::Startup, setup_ui)
      .run();
}
