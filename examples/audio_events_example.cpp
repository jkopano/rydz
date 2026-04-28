// Audio Events Example
// Demonstrates: PlaySoundEvent, AudioPlugin, spatial audio

#include "rl.hpp"
#include "rydz_audio/mod.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_platform/mod.hpp"
#include <print>

using namespace ecs;
using namespace rydz_audio;

// Simple system to trigger sound events on key press
void trigger_sound_events(
  Cmd cmd,
  Res<Input> input,
  Res<Assets<Sound>> sounds
) {
  // This is a placeholder example - in a real application, you would:
  // 1. Load sound assets using AssetServer
  // 2. Store sound handles in a resource or component
  // 3. Trigger events with valid handles
  
  if (input->key_pressed(KEY_SPACE)) {
    std::println("Space pressed - would trigger sound event here");
    
    // Example of how to trigger a sound event:
    // cmd.trigger(PlaySoundEvent{
    //   .sound = my_sound_handle,
    //   .settings = PlaybackSettings{
    //     .looping = false,
    //     .volume = 1.0f,
    //     .spatial = false,
    //     .pitch = 1.0f
    //   },
    //   .position = std::nullopt
    // });
  }
  
  if (input->key_pressed(KEY_S)) {
    std::println("S pressed - would trigger spatial sound event here");
    
    // Example of spatial audio event:
    // cmd.trigger(PlaySoundEvent{
    //   .sound = my_sound_handle,
    //   .settings = PlaybackSettings{
    //     .looping = false,
    //     .volume = 1.0f,
    //     .spatial = true,
    //     .pitch = 1.0f
    //   },
    //   .position = rydz_math::Vec3(5.0f, 0.0f, 0.0f) // 5 units to the right
    // });
  }
}

int main() {
  App app;
  app.add_plugin(rydz_platform::RayPlugin::install({
                     .window = {.width = 800,
                                .height = 600,
                                .title = "Audio Events Example",
                                .target_fps = 60},
                 }))
      .add_plugin(Input::install)
      .add_plugin(AudioPlugin::install)
      .add_systems(Update, trigger_sound_events);
  
  std::println("Audio Events Example");
  std::println("Press SPACE to trigger a sound event");
  std::println("Press S to trigger a spatial sound event");
  std::println("(Note: This example doesn't load actual sounds - see comments in code)");
  
  app.run();
}
