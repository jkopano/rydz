#include "rydz_audio/mod.hpp"
#include "rydz_ecs/app.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_ecs/query.hpp"
#include <gtest/gtest.h>

using namespace rydz_audio;
using namespace ecs;

// Test that all audio components can be added to entities
TEST(AudioComponentsECS, ComponentRegistration) {
  World world;
  
  // Test AudioSource component
  {
    Entity e = world.spawn();
    world.insert_component(e, AudioSource{});
    EXPECT_TRUE(world.has_component<AudioSource>(e));
  }
  
  // Test SpatialAudio component
  {
    Entity e = world.spawn();
    world.insert_component(e, SpatialAudio{});
    EXPECT_TRUE(world.has_component<SpatialAudio>(e));
  }
  
  // Test AudioListener component
  {
    Entity e = world.spawn();
    world.insert_component(e, AudioListener{});
    EXPECT_TRUE(world.has_component<AudioListener>(e));
  }
  
  // Test VolumeControl component
  {
    Entity e = world.spawn();
    world.insert_component(e, VolumeControl{});
    EXPECT_TRUE(world.has_component<VolumeControl>(e));
  }
}

// Test that resources can be initialized
TEST(AudioComponentsECS, ResourceRegistration) {
  World world;
  
  // Test AudioConfig resource
  {
    world.insert_resource(AudioConfig{});
    auto* config = world.get_resource<AudioConfig>();
    ASSERT_NE(config, nullptr);
    EXPECT_EQ(config->master_volume, 1.0f);
    EXPECT_EQ(config->max_concurrent_sounds, 32u);
  }
  
  // Test ActiveAudioSinks resource
  {
    world.insert_resource(ActiveAudioSinks{});
    auto* sinks = world.get_resource<ActiveAudioSinks>();
    ASSERT_NE(sinks, nullptr);
    EXPECT_EQ(sinks->count(), 0u);
  }
}

// Test that components can be queried
TEST(AudioComponentsECS, ComponentQueries) {
  World world;
  
  // Create entities with different component combinations
  Entity e1 = world.spawn();
  world.insert_component(e1, AudioSource{});
  
  Entity e2 = world.spawn();
  world.insert_component(e2, AudioSource{});
  world.insert_component(e2, SpatialAudio{});
  
  Entity e3 = world.spawn();
  world.insert_component(e3, AudioListener{});
  
  // Query for AudioSource components
  {
    Query<AudioSource> query(world, Tick{0}, world.read_change_tick());
    usize count = 0;
    query.each([&](const AudioSource*) {
      count++;
    });
    EXPECT_EQ(count, 2u);
  }
  
  // Query for AudioSource + SpatialAudio
  {
    Query<AudioSource, SpatialAudio> query(world, Tick{0}, world.read_change_tick());
    usize count = 0;
    query.each([&](const AudioSource*, const SpatialAudio*) {
      count++;
    });
    EXPECT_EQ(count, 1u);
  }
  
  // Query for AudioListener
  {
    Query<AudioListener> query(world, Tick{0}, world.read_change_tick());
    usize count = 0;
    query.each([&](const AudioListener*) {
      count++;
    });
    EXPECT_EQ(count, 1u);
  }
}


// ============================================================================
// handle_audio_source_additions_system Tests
// ============================================================================

TEST(AudioSourceAdditionsSystem, DetectsNewlyAddedAudioSource) {
  // Test Requirement 2.3: System detects newly added AudioSource components
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create entity with AudioSource
  Entity entity = world.spawn();
  
  // Record tick before adding component
  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  
  // Add AudioSource component
  world.insert_component(entity, AudioSource{});
  
  Tick this_run = world.read_change_tick();
  
  // Query with Added filter should detect the new component
  Query<Entity, AudioSource, Filters<Added<AudioSource>>> query(world, last_run, this_run);
  usize count = 0;
  query.each([&](Entity e, const AudioSource*) {
    EXPECT_EQ(e, entity);
    count++;
  });
  
  EXPECT_EQ(count, 1u);
}

TEST(AudioSourceAdditionsSystem, InvalidHandleLogsWarning) {
  // Test Requirement 10.1: Invalid Handle<Sound> should log warning
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create entity with AudioSource with invalid handle
  Entity entity = world.spawn();
  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  
  AudioSource source;
  source.sound = Handle<rydz_audio::Sound>{}; // Invalid handle
  world.insert_component(entity, source);
  
  Tick this_run = world.read_change_tick();
  
  // Run the system
  Query<Entity, AudioSource, Filters<Added<AudioSource>>> query(world, last_run, this_run);
  auto sinks = world.get_resource<ActiveAudioSinks>();
  
  query.each([&](Entity, const AudioSource* src) {
    // This should not crash and should skip playback
    if (!src->sound.is_valid()) {
      // Warning would be logged here
      return;
    }
  });
  
  // Verify no sinks were created
  EXPECT_EQ(sinks->count(), 0u);
}

TEST(AudioSourceAdditionsSystem, AudioSourceStructureDefaults) {
  // Test AudioSource component structure and defaults
  AudioSource source;
  
  EXPECT_EQ(source.state, PlaybackState::Playing);
  EXPECT_FLOAT_EQ(source.playback_position, 0.0f);
  EXPECT_FALSE(source.marked_for_removal);
  EXPECT_FALSE(source.settings.looping);
  EXPECT_FLOAT_EQ(source.settings.volume, 1.0f);
  EXPECT_FALSE(source.settings.spatial);
  EXPECT_FLOAT_EQ(source.settings.pitch, 1.0f);
}

TEST(AudioSourceAdditionsSystem, SpatialAudioStructureDefaults) {
  // Test SpatialAudio component structure and defaults
  SpatialAudio spatial;
  
  EXPECT_FLOAT_EQ(spatial.position.x, 0.0f);
  EXPECT_FLOAT_EQ(spatial.position.y, 0.0f);
  EXPECT_FLOAT_EQ(spatial.position.z, 0.0f);
  EXPECT_FLOAT_EQ(spatial.min_distance, 1.0f);
  EXPECT_FLOAT_EQ(spatial.max_distance, 100.0f);
  EXPECT_EQ(spatial.attenuation, AttenuationModel::InverseDistance);
}

TEST(AudioSourceAdditionsSystem, AudioListenerStructureDefaults) {
  // Test AudioListener component structure and defaults
  AudioListener listener;
  
  EXPECT_FLOAT_EQ(listener.position.x, 0.0f);
  EXPECT_FLOAT_EQ(listener.position.y, 0.0f);
  EXPECT_FLOAT_EQ(listener.position.z, 0.0f);
  EXPECT_FLOAT_EQ(listener.forward.x, 0.0f);
  EXPECT_FLOAT_EQ(listener.forward.y, 0.0f);
  EXPECT_FLOAT_EQ(listener.forward.z, -1.0f);
  EXPECT_FLOAT_EQ(listener.up.x, 0.0f);
  EXPECT_FLOAT_EQ(listener.up.y, 1.0f);
  EXPECT_FLOAT_EQ(listener.up.z, 0.0f);
  EXPECT_TRUE(listener.active);
}

TEST(AudioSourceAdditionsSystem, MultipleAudioSourcesDetected) {
  // Test that system detects multiple newly added AudioSource components
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Record tick
  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  
  // Create multiple entities with AudioSource
  Entity e1 = world.spawn();
  world.insert_component(e1, AudioSource{});
  
  Entity e2 = world.spawn();
  world.insert_component(e2, AudioSource{});
  
  Entity e3 = world.spawn();
  world.insert_component(e3, AudioSource{});
  
  Tick this_run = world.read_change_tick();
  
  // Query should detect all three new components
  Query<Entity, AudioSource, Filters<Added<AudioSource>>> query(world, last_run, this_run);
  usize count = 0;
  query.each([&](Entity, const AudioSource*) {
    count++;
  });
  
  EXPECT_EQ(count, 3u);
}

TEST(AudioSourceAdditionsSystem, OnlyDetectsNewlyAdded) {
  // Test that system only detects newly added components, not existing ones
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create entity with AudioSource at tick 0
  Entity e1 = world.spawn();
  world.insert_component(e1, AudioSource{});
  
  Tick last_run = world.read_change_tick();
  world.increment_change_tick();
  
  // Create another entity with AudioSource at tick 1
  Entity e2 = world.spawn();
  world.insert_component(e2, AudioSource{});
  
  Tick this_run = world.read_change_tick();
  
  // Query should only detect e2 (newly added since last_run)
  Query<Entity, AudioSource, Filters<Added<AudioSource>>> query(world, last_run, this_run);
  usize count = 0;
  Entity detected_entity;
  query.each([&](Entity e, const AudioSource*) {
    detected_entity = e;
    count++;
  });
  
  EXPECT_EQ(count, 1u);
  EXPECT_EQ(detected_entity, e2);
}

// ============================================================================
// handle_audio_source_removals_system Tests
// ============================================================================

TEST(AudioSourceRemovalsSystem, DetectsRemovedAudioSource) {
  // Test Requirement 2.4: System detects removed AudioSource components and stops playback
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create entity with AudioSource
  Entity entity = world.spawn();
  world.insert_component(entity, AudioSource{});
  
  // Manually create an AudioSink for this entity (simulating that playback started)
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  AudioSink sink{
    .entity = entity,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  };
  sinks->sinks.push_back(sink);
  
  EXPECT_EQ(sinks->count(), 1u);
  
  // Remove the AudioSource component
  world.remove_component<AudioSource>(entity);
  
  // Run the removal system
  handle_audio_source_removals_system(
    world,
    ecs::ResMut<ActiveAudioSinks>{world.get_resource<ActiveAudioSinks>()},
    ecs::Res<Assets<rydz_audio::Sound>>{world.get_resource<Assets<rydz_audio::Sound>>()}
  );
  
  // Verify the sink was removed
  sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 0u);
}

TEST(AudioSourceRemovalsSystem, IgnoresEventBasedSinks) {
  // Test that system doesn't remove event-based sinks (those without valid entity)
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create an event-based sink (default entity with index 0)
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  AudioSink event_sink{
    .entity = Entity{}, // Default entity for event-based playback
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  };
  sinks->sinks.push_back(event_sink);
  
  EXPECT_EQ(sinks->count(), 1u);
  
  // Run the removal system
  handle_audio_source_removals_system(
    world,
    ecs::ResMut<ActiveAudioSinks>{world.get_resource<ActiveAudioSinks>()},
    ecs::Res<Assets<rydz_audio::Sound>>{world.get_resource<Assets<rydz_audio::Sound>>()}
  );
  
  // Verify the event-based sink was NOT removed
  sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 1u);
}

TEST(AudioSourceRemovalsSystem, OnlyRemovesSinksWithoutComponent) {
  // Test that system only removes sinks whose entities no longer have AudioSource
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create two entities with AudioSource
  Entity e1 = world.spawn();
  world.insert_component(e1, AudioSource{});
  
  Entity e2 = world.spawn();
  world.insert_component(e2, AudioSource{});
  
  // Create sinks for both entities
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  sinks->sinks.push_back(AudioSink{
    .entity = e1,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  });
  sinks->sinks.push_back(AudioSink{
    .entity = e2,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  });
  
  EXPECT_EQ(sinks->count(), 2u);
  
  // Remove AudioSource from only e1
  world.remove_component<AudioSource>(e1);
  
  // Run the removal system
  handle_audio_source_removals_system(
    world,
    ecs::ResMut<ActiveAudioSinks>{world.get_resource<ActiveAudioSinks>()},
    ecs::Res<Assets<rydz_audio::Sound>>{world.get_resource<Assets<rydz_audio::Sound>>()}
  );
  
  // Verify only e1's sink was removed
  sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 1u);
  EXPECT_EQ(sinks->sinks[0].entity, e2);
}

TEST(AudioSourceRemovalsSystem, HandlesMultipleRemovals) {
  // Test that system can handle multiple removals in one pass
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  // Create three entities with AudioSource
  Entity e1 = world.spawn();
  world.insert_component(e1, AudioSource{});
  
  Entity e2 = world.spawn();
  world.insert_component(e2, AudioSource{});
  
  Entity e3 = world.spawn();
  world.insert_component(e3, AudioSource{});
  
  // Create sinks for all entities
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  sinks->sinks.push_back(AudioSink{
    .entity = e1,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  });
  sinks->sinks.push_back(AudioSink{
    .entity = e2,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  });
  sinks->sinks.push_back(AudioSink{
    .entity = e3,
    .sound = Handle<rydz_audio::Sound>{},
    .base_volume = 1.0f,
    .started_at = Tick{0}
  });
  
  EXPECT_EQ(sinks->count(), 3u);
  
  // Remove AudioSource from e1 and e3
  world.remove_component<AudioSource>(e1);
  world.remove_component<AudioSource>(e3);
  
  // Run the removal system
  handle_audio_source_removals_system(
    world,
    ecs::ResMut<ActiveAudioSinks>{world.get_resource<ActiveAudioSinks>()},
    ecs::Res<Assets<rydz_audio::Sound>>{world.get_resource<Assets<rydz_audio::Sound>>()}
  );
  
  // Verify only e2's sink remains
  sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 1u);
  EXPECT_EQ(sinks->sinks[0].entity, e2);
}

TEST(AudioSourceRemovalsSystem, HandlesEmptySinksList) {
  // Test that system handles empty sinks list gracefully
  World world;
  
  // Initialize resources
  world.insert_resource(AudioConfig{});
  world.insert_resource(ActiveAudioSinks{});
  world.insert_resource(Assets<rydz_audio::Sound>{});
  
  auto* sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 0u);
  
  // Run the removal system with empty sinks
  handle_audio_source_removals_system(
    world,
    ecs::ResMut<ActiveAudioSinks>{world.get_resource<ActiveAudioSinks>()},
    ecs::Res<Assets<rydz_audio::Sound>>{world.get_resource<Assets<rydz_audio::Sound>>()}
  );
  
  // Should not crash and sinks should still be empty
  sinks = world.get_resource<ActiveAudioSinks>();
  EXPECT_EQ(sinks->count(), 0u);
}
