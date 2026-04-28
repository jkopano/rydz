#pragma once

#include "rl.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/ticks.hpp"
#include "rydz_math/mod.hpp"
#include "types.hpp"
#include <algorithm>
#include <optional>
#include <vector>

namespace rydz_audio {

struct Sound;

enum class PlaybackState { Playing, Paused, Stopped };

enum class PlaybackMode { OneShot, Loop, Once };

enum class AttenuationModel { Linear, InverseDistance, Exponential };

struct PlaybackSettings {
  bool looping = false;
  f32 volume = 1.0f;
  bool spatial = false;
  f32 pitch = 1.0f;

  [[nodiscard]] auto clamped_volume() const -> f32 {
    return std::clamp(volume, 0.0f, 1.0f);
  }
};

struct PlaySoundEvent {
  using T = ecs::Event;

  ecs::Handle<Sound> sound;
  PlaybackSettings settings;
  std::optional<rydz_math::Vec3> position;
};

struct AudioSource {
  ecs::Handle<Sound> sound;
  PlaybackSettings settings;
  PlaybackState state = PlaybackState::Playing;
  f32 playback_position = 0.0f;
  bool marked_for_removal = false;
};

struct SpatialAudio {
  rydz_math::Vec3 position{0.0f};
  f32 min_distance = 1.0f;
  f32 max_distance = 100.0f;
  AttenuationModel attenuation = AttenuationModel::InverseDistance;
};

struct AudioListener {
  rydz_math::Vec3 position{0.0f};
  rydz_math::Vec3 forward{0.0f, 0.0f, -1.0f};
  rydz_math::Vec3 up{0.0f, 1.0f, 0.0f};
  bool active = true;
};

struct VolumeControl {
  f32 volume = 1.0f;
  [[nodiscard]] auto clamped_volume() const -> f32 {
    return std::clamp(volume, 0.0f, 1.0f);
  }
};

struct AudioConfig {
  using T = ecs::Resource;

  f32 master_volume = 1.0f;
  u32 max_concurrent_sounds = 32;
  f32 default_min_distance = 1.0f;
  f32 default_max_distance = 100.0f;
  AttenuationModel default_attenuation = AttenuationModel::InverseDistance;
};

struct AudioSink {
  ecs::Entity entity;
  ecs::Handle<Sound> sound;
  f32 base_volume = 1.0f;
  ecs::Tick started_at;
};

struct ActiveAudioSinks {
  using T = ecs::Resource;

  std::vector<AudioSink> sinks;

  [[nodiscard]] auto count() const -> usize { return sinks.size(); }

  auto oldest() -> AudioSink* {
    if (sinks.empty())
      return nullptr;
    return &sinks.front();
  }

  [[nodiscard]] auto count_active_sources() const -> usize {
    usize count = 0;
    for (auto const& sink : sinks) {

      if (sink.entity.index() != 0) {
        count++;
      }
    }
    return count;
  }

  [[nodiscard]] auto count_event_sounds() const -> usize {
    usize count = 0;
    for (auto const& sink : sinks) {

      if (sink.entity.index() == 0) {
        count++;
      }
    }
    return count;
  }
};

} // namespace rydz_audio
