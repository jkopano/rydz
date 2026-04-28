#include "rl.hpp"
#include "rydz_audio/mod.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/mod.hpp"
#include "rydz_graphics/plugin.hpp"
#include "rydz_math/mod.hpp"
#include "rydz_platform/mod.hpp"

using namespace ecs;
using namespace rydz_audio;
using namespace rydz_math;

struct AudioExampleState {
  using T = Resource;

  Handle<rydz_audio::Sound> ping;
  bool sound_ready_logged = false;
};

namespace {
auto setup_scene(Cmd cmd, Res<AssetServer> asset_server, ResMut<AudioExampleState> state)
  -> void {
  state->ping = asset_server->load<rydz_audio::Sound>("res/sounds/ping.mp3");

  cmd.spawn(
    Camera3d::perspective(60.0f),
    ActiveCamera{},
    ClearColor{{18, 22, 30, 255}},
    Transform::from_xyz(0.0f, 3.0f, 6.0f).look_at(Vec3::ZERO)
  );

  cmd.spawn(
    AudioListener{
      .position = Vec3::ZERO,
      .forward = Vec3{0.0f, 0.0f, -1.0f},
      .up = Vec3{0.0f, 1.0f, 0.0f},
      .active = true,
    }
  );

  info("14 - Audio System");
  info("Loading: res/sounds/ping.mp3");
  info("SPACE: play centered ping");
  info("A: play ping from left");
  info("D: play ping from right");
  info("W: play higher-pitch ping");
}

auto play_example_sounds(
  Cmd cmd,
  Res<Input> input,
  Res<Assets<rydz_audio::Sound>> sounds,
  ResMut<AudioExampleState> state
) -> void {
  if (!state->ping.is_valid()) {
    return;
  }

  auto const* ping = sounds->get(state->ping);
  if (ping == nullptr || !ping->ready()) {
    return;
  }

  if (!state->sound_ready_logged) {
    state->sound_ready_logged = true;
    info("Sound ready. Press SPACE, A, D or W.");
    if (!ping->is_mono()) {
      info(
        "res/sounds/ping.mp3 has {} channels; positional pan is balance-based, not true "
        "mono 3D.",
        ping->channels()
      );
    }
  }

  if (input->key_pressed(KEY_SPACE)) {
    cmd.trigger(
      PlaySoundEvent{
        .sound = state->ping,
        .settings =
          PlaybackSettings{
            .looping = false,
            .volume = 1.0f,
            .spatial = false,
            .pitch = 1.0f,
          },
        .position = {},
      }
    );
  }

  if (input->key_pressed(KEY_A)) {
    cmd.trigger(
      PlaySoundEvent{
        .sound = state->ping,
        .settings =
          PlaybackSettings{
            .looping = false,
            .volume = 1.0f,
            .spatial = true,
            .pitch = 1.0f,
          },
        .position = Vec3{-10.0f, 0.0f, 0.0f},
      }
    );
  }

  if (input->key_pressed(KEY_D)) {
    cmd.trigger(
      PlaySoundEvent{
        .sound = state->ping,
        .settings =
          PlaybackSettings{
            .looping = false,
            .volume = 1.0f,
            .spatial = true,
            .pitch = 1.0f,
          },
        .position = Vec3{10.0f, 0.0f, 0.0f},
      }
    );
  }

  if (input->key_pressed(KEY_W)) {
    cmd.trigger(
      PlaySoundEvent{
        .sound = state->ping,
        .settings =
          PlaybackSettings{
            .looping = false,
            .volume = 0.9f,
            .spatial = false,
            .pitch = 1.35f,
          },
        .position = std::nullopt,
      }
    );
  }
}
} // namespace

auto main() -> int {
  App app;
  app
    .add_plugin(
      rydz_platform::RayPlugin{
        .window =
          {.width = 960, .height = 540, .title = "14 - Audio System", .target_fps = 60},
      }
    )
    .add_plugin(time_plugin)
    .add_plugin(RenderPlugin{})
    .add_plugin(InputPlugin{})
    .init_resource<AudioExampleState>()
    .add_plugin(AudioPlugin{})
    .add_systems(Startup, setup_scene)
    .add_systems(Update, play_example_sounds)
    .run();

  return 0;
}
