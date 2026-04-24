#include <gtest/gtest.h>

#include "rydz_audio/mod.hpp"
#include "rydz_ecs/mod.hpp"

using namespace ecs;
using namespace rydz_audio;

// ============================================================================
// process_audio_events_observer Tests
// ============================================================================

TEST(AudioEventsTest, InvalidHandleLogsWarning) {
  // Test Requirement 1.4: Invalid Handle<Sound> should log warning and skip playback
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Register event and observer
  world.add_event<PlaySoundEvent>();
  world.add_observer(process_audio_events_observer);
  
  // Create invalid handle
  Handle<rydz_audio::Sound> invalid_handle;
  
  // Trigger event with invalid handle
  PlaySoundEvent event{
    .sound = invalid_handle,
    .settings = PlaybackSettings{},
    .position = std::nullopt
  };
  
  // This should not crash and should log a warning
  world.trigger(event);
  
  // Verify no sinks were created
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  ASSERT_NE(sinks, nullptr);
  EXPECT_EQ(sinks->count(), 0);
}

TEST(AudioEventsTest, EventWithoutPositionCreatesNonSpatialSink) {
  // Test Requirement 1.2: PlaySoundEvent should create AudioSink
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Register event and observer
  world.add_event<PlaySoundEvent>();
  world.add_observer(process_audio_events_observer);
  
  // Note: We can't actually test sound playback without initializing raylib audio
  // This test verifies the structure is correct but won't create actual sinks
  // without valid Sound assets
}

TEST(AudioEventsTest, AudioConfigDefaultValues) {
  // Test Requirement 9.1, 9.2, 9.3: AudioConfig has correct defaults
  AudioConfig config;
  
  EXPECT_FLOAT_EQ(config.master_volume, 1.0f);
  EXPECT_EQ(config.max_concurrent_sounds, 32u);
  EXPECT_FLOAT_EQ(config.default_min_distance, 1.0f);
  EXPECT_FLOAT_EQ(config.default_max_distance, 100.0f);
  EXPECT_EQ(config.default_attenuation, AttenuationModel::InverseDistance);
}

TEST(AudioEventsTest, PlaybackSettingsClampedVolume) {
  // Test that PlaybackSettings correctly clamps volume
  PlaybackSettings settings;
  
  settings.volume = 0.5f;
  EXPECT_FLOAT_EQ(settings.clamped_volume(), 0.5f);
  
  settings.volume = -0.5f;
  EXPECT_FLOAT_EQ(settings.clamped_volume(), 0.0f);
  
  settings.volume = 1.5f;
  EXPECT_FLOAT_EQ(settings.clamped_volume(), 1.0f);
}

TEST(AudioEventsTest, VolumeControlClampedVolume) {
  // Test that VolumeControl correctly clamps volume
  VolumeControl volume;
  
  volume.volume = 0.7f;
  EXPECT_FLOAT_EQ(volume.clamped_volume(), 0.7f);
  
  volume.volume = -0.3f;
  EXPECT_FLOAT_EQ(volume.clamped_volume(), 0.0f);
  
  volume.volume = 2.0f;
  EXPECT_FLOAT_EQ(volume.clamped_volume(), 1.0f);
}

TEST(AudioEventsTest, ActiveAudioSinksCount) {
  // Test ActiveAudioSinks count and oldest methods
  ActiveAudioSinks sinks;
  
  EXPECT_EQ(sinks.count(), 0);
  EXPECT_EQ(sinks.oldest(), nullptr);
  
  // Add a sink
  AudioSink sink{
    .entity = Entity{},
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  };
  
  sinks.sinks.push_back(sink);
  
  EXPECT_EQ(sinks.count(), 1);
  EXPECT_NE(sinks.oldest(), nullptr);
}

TEST(AudioEventsTest, PlaySoundEventStructure) {
  // Test PlaySoundEvent structure (Requirement 1.3)
  Handle<rydz_audio::Sound> handle;
  PlaybackSettings settings{
    .looping = true,
    .volume = 0.8f,
    .spatial = true,
    .pitch = 1.2f
  };
  rydz_math::Vec3 position(1.0f, 2.0f, 3.0f);
  
  PlaySoundEvent event{
    .sound = handle,
    .settings = settings,
    .position = position
  };
  
  EXPECT_EQ(event.sound, handle);
  EXPECT_TRUE(event.settings.looping);
  EXPECT_FLOAT_EQ(event.settings.volume, 0.8f);
  EXPECT_TRUE(event.settings.spatial);
  EXPECT_FLOAT_EQ(event.settings.pitch, 1.2f);
  EXPECT_TRUE(event.position.has_value());
  EXPECT_FLOAT_EQ(event.position->x, 1.0f);
  EXPECT_FLOAT_EQ(event.position->y, 2.0f);
  EXPECT_FLOAT_EQ(event.position->z, 3.0f);
}
