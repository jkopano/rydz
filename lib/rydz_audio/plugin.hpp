#pragma once

#include "rl.hpp"
#include "rydz_audio/loaders.hpp"
#include "rydz_audio/systems.hpp"
#include "rydz_audio/types.hpp"
#include "rydz_ecs/app.hpp"

namespace rydz_audio {

struct AudioDeviceState {
  using T = ecs::Resource;

  AudioDeviceState() {
    rl::InitAudioDevice();
    owns_device_ = rl::IsAudioDeviceReady();
  }

  AudioDeviceState(AudioDeviceState&& other) noexcept
      : owns_device_(other.owns_device_) {
    other.owns_device_ = false;
  }

  auto operator=(AudioDeviceState&& other) noexcept -> AudioDeviceState& {
    if (this == &other) {
      return *this;
    }

    release();
    owns_device_ = other.owns_device_;
    other.owns_device_ = false;
    return *this;
  }

  AudioDeviceState(AudioDeviceState const&) = delete;
  auto operator=(AudioDeviceState const&) -> AudioDeviceState& = delete;

  ~AudioDeviceState() { release(); }

private:
  bool owns_device_ = false;

  auto release() -> void {
    if (!owns_device_) {
      return;
    }
    if (rl::IsAudioDeviceReady()) {
      rl::CloseAudioDevice();
    }
    owns_device_ = false;
  }
};

struct AudioPlugin : ecs::IPlugin {
  auto build(ecs::App& app) const -> void {
    app.init_resource<ecs::AssetServer>();
    app.init_resource<AudioDeviceState>();
    app.init_resource<ecs::Assets<Sound>>([](Sound& sound) -> void { sound.unload(); });

    if (auto* server = app.world().get_resource<ecs::AssetServer>()) {
      register_audio_loaders(*server);
    }

    app.insert_resource(AudioConfig{});
    app.insert_resource(ActiveAudioSinks{});

    app.add_event<PlaySoundEvent>();

    app.add_observer(process_audio_events_observer);

    app.add_systems(ecs::First, [](ecs::World& world) -> void {
      if (auto* server = world.get_resource<ecs::AssetServer>()) {
        server->update(world);
      }
    });

    app.add_systems(ecs::Update, handle_audio_source_additions_system);
    app.add_systems(ecs::Update, handle_audio_source_removals_system);
    app.add_systems(ecs::Update, update_playback_state_system);
    app.add_systems(ecs::Update, update_spatial_audio_system);
    app.add_systems(ecs::Update, update_volume_system);
    app.add_systems(ecs::Update, cleanup_finished_sounds_system);
    app.add_systems(ecs::Update, enforce_concurrent_limit_system);
  }
};

} // namespace rydz_audio
