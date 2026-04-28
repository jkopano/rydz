#pragma once

#include "audio.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/entity.hpp"
#include "rydz_ecs/event.hpp"
#include "rydz_ecs/query.hpp"
#include "rydz_ecs/resource.hpp"
#include "rydz_ecs/world.hpp"
#include "rydz_log/mod.hpp"
#include "rydz_math/mod.hpp"
#include "spatial.hpp"
#include "types.hpp"

using namespace ecs;

namespace rydz_audio {

[[nodiscard]] inline auto get_sound_asset(
  ecs::Assets<Sound> const& assets, ecs::Handle<Sound> handle
) -> Sound const* {
  if (!handle.is_valid()) {
    return nullptr;
  }
  auto const* sound = assets.get(handle);
  if (sound == nullptr || !sound->ready()) {
    return nullptr;
  }
  return sound;
}

inline auto stop_sink_if_loaded(AudioSink const& sink, ecs::Assets<Sound> const& assets)
  -> void {
  if (auto const* sound = get_sound_asset(assets, sink.sound)) {
    sound->stop();
  }
}

inline auto process_audio_events_observer(
  On<PlaySoundEvent> event,
  Res<AudioConfig> config,
  ResMut<ActiveAudioSinks> sinks,
  Res<ecs::Assets<Sound>> assets,
  Query<AudioListener const> listener_query
) -> void {

  if (!event->sound.is_valid()) {
    debug("[rydz_audio] Warning: PlaySoundEvent received with invalid Handle<Sound>");
    return;
  }

  auto const* sound_asset = get_sound_asset(*assets, event->sound);
  if (sound_asset == nullptr) {
    debug(
      "[rydz_audio] Warning: PlaySoundEvent references unloaded or invalid Sound asset"
    );
    return;
  }

  if (sinks->count() >= config->max_concurrent_sounds) {
    debug(
      "[rydz_audio] Warning: Maximum concurrent sounds limit ({}) reached, stopping "
      "oldest sound",
      config->max_concurrent_sounds
    );

    if (auto* oldest = sinks->oldest()) {
      stop_sink_if_loaded(*oldest, *assets);
      sinks->sinks.erase(sinks->sinks.begin());
    }
  }

  sound_asset->set_volume(event->settings.clamped_volume() * config->master_volume);
  sound_asset->set_pitch(event->settings.pitch);
  sound_asset->set_pan(0.0f);

  if (event->settings.spatial && event->position.has_value()) {
    rydz_math::Vec3 listener_pos{0.0f, 0.0f, 0.0f};
    rydz_math::Vec3 listener_forward{0.0f, 0.0f, -1.0f};
    rydz_math::Vec3 listener_up{0.0f, 1.0f, 0.0f};

    for (auto [listener] : listener_query.iter()) {
      if (!listener->active) {
        continue;
      }
      listener_pos = listener->position;
      listener_forward = listener->forward;
      listener_up = listener->up;
      break;
    }

    auto const source_pos = event->position.value();
    auto [attenuation, panning] = apply_spatial_audio(
      source_pos,
      listener_pos,
      listener_forward,
      listener_up,
      config->default_min_distance,
      config->default_max_distance,
      config->default_attenuation
    );

    f32 spatial_volume =
      event->settings.clamped_volume() * config->master_volume * attenuation;
    sound_asset->set_volume(spatial_volume);
    sound_asset->set_pan(panning);
  }

  sound_asset->play();

  AudioSink sink{
    .entity = ecs::Entity{},
    .sound = event->sound,
    .base_volume = event->settings.clamped_volume(),
    .started_at = ecs::Tick{0}
  };

  sinks->sinks.push_back(sink);

  debug(
    "[rydz_audio] Started sound playback (event-based), total active: {}", sinks->count()
  );
}

inline auto handle_audio_source_additions_system(
  Query<ecs::Entity, AudioSource, Added<AudioSource>> query,
  Res<AudioConfig> config,
  ResMut<ActiveAudioSinks> sinks,
  Res<ecs::Assets<Sound>> assets
) -> void {
  for (auto [entity, source] : query.iter()) {
    if (!source->sound.is_valid()) {
      debug(
        "[rydz_audio] Warning: AudioSource on entity {} has invalid Handle<Sound>",
        entity.index()
      );
      return;
    }

    auto const* sound_asset = get_sound_asset(*assets, source->sound);
    if (sound_asset == nullptr) {
      debug(
        "[rydz_audio] Warning: AudioSource on entity {} references unloaded or invalid "
        "Sound asset",
        entity.index()
      );
      return;
    }

    if (sinks->count() >= config->max_concurrent_sounds) {
      debug(
        "[rydz_audio] Warning: Maximum concurrent sounds limit ({}) reached, stopping "
        "oldest sound",
        config->max_concurrent_sounds
      );

      if (auto* oldest = sinks->oldest()) {
        stop_sink_if_loaded(*oldest, *assets);
        sinks->sinks.erase(sinks->sinks.begin());
      }
    }

    sound_asset->set_volume(source->settings.clamped_volume() * config->master_volume);
    sound_asset->set_pitch(source->settings.pitch);

    if (source->settings.looping) {
    }

    sound_asset->play();

    AudioSink sink{
      .entity = entity,
      .sound = source->sound,
      .base_volume = source->settings.clamped_volume(),
      .started_at = ecs::Tick{0}
    };

    sinks->sinks.push_back(sink);

    debug(
      "[rydz_audio] Started sound playback for entity {}, total active: {}",
      entity.index(),
      sinks->count()
    );
  }
}

inline auto handle_audio_source_removals_system(
  ecs::World& world,
  ecs::ResMut<ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<Sound>> assets
) -> void {

  for (auto it = sinks->sinks.rbegin(); it != sinks->sinks.rend();) {
    auto& sink = *it;

    if (sink.entity.index() == 0) {
      ++it;
      continue;
    }

    if (!world.has_component<AudioSource>(sink.entity)) {
      stop_sink_if_loaded(sink, *assets);

      debug(
        "[rydz_audio] Stopped sound playback for removed AudioSource on entity {}, "
        "total active: {}",
        sink.entity.index(),
        sinks->count() - 1
      );

      auto forward_it = std::next(it).base();
      sinks->sinks.erase(forward_it);

      it = std::reverse_iterator(forward_it);
    } else {
      ++it;
    }
  }
}

} // namespace rydz_audio

inline auto update_playback_state_system(
  ecs::Query<ecs::Entity, ecs::Mut<rydz_audio::AudioSource>> query,
  ecs::Res<rydz_audio::AudioConfig> config,
  ecs::ResMut<rydz_audio::ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<rydz_audio::Sound>> assets
) -> void {
  for (auto [entity, source] : query.iter()) {
    auto sink_it = std::find_if(
      sinks->sinks.begin(),
      sinks->sinks.end(),
      [entity](rydz_audio::AudioSink const& sink) -> bool {
        return sink.entity == entity;
      }
    );

    if (sink_it == sinks->sinks.end()) {

      continue;
    }

    auto& sink = *sink_it;
    auto const* sound = rydz_audio::get_sound_asset(*assets, sink.sound);
    if (sound == nullptr) {
      continue;
    }

    switch (source->state) {
    case rydz_audio::PlaybackState::Playing:

      if (!sound->is_playing()) {
        sound->resume();

        debug("[rydz_audio] Resumed sound playback for entity {}", entity.index());
      }
      break;

    case rydz_audio::PlaybackState::Paused:

      if (sound->is_playing()) {
        sound->pause();

        debug("[rydz_audio] Paused sound playback for entity {}", entity.index());
      }
      break;

    case rydz_audio::PlaybackState::Stopped:

      sound->stop();
      source->playback_position = 0.0f;

      debug("[rydz_audio] Stopped sound playback for entity {}", entity.index());
      break;
    }
  }
}

inline auto update_spatial_audio_system(
  ecs::Query<ecs::Entity, rydz_audio::AudioSource const, rydz_audio::SpatialAudio const>
    spatial_query,
  ecs::Query<rydz_audio::AudioListener const> listener_query,
  ecs::Res<rydz_audio::AudioConfig> config,
  ecs::ResMut<rydz_audio::ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<rydz_audio::Sound>> assets
) -> void {

  rydz_math::Vec3 listener_pos{0.0f, 0.0f, 0.0f};
  rydz_math::Vec3 listener_forward{0.0f, 0.0f, -1.0f};
  rydz_math::Vec3 listener_up{0.0f, 1.0f, 0.0f};

  bool found_listener = false;
  for (auto [listener] : listener_query.iter()) {
    if (!found_listener && listener->active) {
      listener_pos = listener->position;
      listener_forward = listener->forward;
      listener_up = listener->up;
      found_listener = true;
    }
  }

  for (auto [entity, source, spatial] : spatial_query.iter()) {
    auto sink_it = std::find_if(
      sinks->sinks.begin(),
      sinks->sinks.end(),
      [entity](rydz_audio::AudioSink const& sink) -> bool {
        return sink.entity == entity;
      }
    );

    if (sink_it == sinks->sinks.end()) {

      continue;
    }

    auto& sink = *sink_it;
    auto const* sound = rydz_audio::get_sound_asset(*assets, sink.sound);
    if (sound == nullptr) {
      continue;
    }

    auto [attenuation, panning] = apply_spatial_audio(
      spatial->position,
      listener_pos,
      listener_forward,
      listener_up,
      spatial->min_distance,
      spatial->max_distance,
      spatial->attenuation
    );

    f32 final_volume =
      config->master_volume * source->settings.clamped_volume() * attenuation;

    sound->set_volume(final_volume);
    sound->set_pan(panning);
  }
}

inline auto update_volume_system(
  ecs::Query<ecs::Entity, rydz_audio::AudioSource const> query,
  ecs::World& world,
  ecs::Res<rydz_audio::AudioConfig> config,
  ecs::ResMut<rydz_audio::ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<rydz_audio::Sound>> assets
) -> void {
  for (auto [entity, source] : query.iter()) {
    auto sink_it = std::find_if(
      sinks->sinks.begin(),
      sinks->sinks.end(),
      [entity](rydz_audio::AudioSink const& sink) -> bool {
        return sink.entity == entity;
      }
    );

    if (sink_it == sinks->sinks.end()) {

      continue;
    }

    auto& sink = *sink_it;
    auto const* sound = rydz_audio::get_sound_asset(*assets, sink.sound);
    if (sound == nullptr) {
      continue;
    }

    f32 volume_control_multiplier = 1.0f;
    if (auto* volume_ctrl = world.get_component<rydz_audio::VolumeControl>(entity)) {
      volume_control_multiplier = volume_ctrl->clamped_volume();
    }

    f32 final_volume = config->master_volume * source->settings.clamped_volume() *
                       volume_control_multiplier;

    sound->set_volume(final_volume);
  }
}

inline auto cleanup_finished_sounds_system(
  ecs::World& world,
  ecs::Res<rydz_audio::AudioConfig> config,
  ecs::ResMut<rydz_audio::ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<rydz_audio::Sound>> assets
) -> void {

  for (auto it = sinks->sinks.begin(); it != sinks->sinks.end();) {
    auto& sink = *it;
    auto const* sound = rydz_audio::get_sound_asset(*assets, sink.sound);
    if (sound == nullptr) {
      it = sinks->sinks.erase(it);
      continue;
    }

    if (sink.entity.index() == 0) {

      if (!sound->is_playing()) {
        debug("[rydz_audio] Event-based sound finished, removing sink");
        it = sinks->sinks.erase(it);
      } else {
        ++it;
      }
      continue;
    }

    if (sound->is_playing()) {
      ++it;
      continue;
    }

    auto* source = world.get_component<rydz_audio::AudioSource>(sink.entity);
    if (source == nullptr) {

      it = sinks->sinks.erase(it);
      continue;
    }

    if (source->settings.looping) {

      sound->play();
      ++it;
      continue;
    }

    if (source->marked_for_removal) {

      world.remove_component<rydz_audio::AudioSource>(sink.entity);

      debug(
        "[rydz_audio] Removed AudioSource from entity {} after playback finished",
        sink.entity.index()
      );

      it = sinks->sinks.erase(it);
    } else {

      source->state = rydz_audio::PlaybackState::Stopped;
      source->playback_position = 0.0f;

      it = sinks->sinks.erase(it);
    }
  }
}

inline auto enforce_concurrent_limit_system(
  ecs::Res<rydz_audio::AudioConfig> config,
  ecs::ResMut<rydz_audio::ActiveAudioSinks> sinks,
  ecs::Res<ecs::Assets<rydz_audio::Sound>> assets
) -> void {

  while (sinks->count() > config->max_concurrent_sounds) {

    debug(
      "[rydz_audio] Warning: Concurrent sounds limit ({}) exceeded, stopping oldest "
      "sound",
      config->max_concurrent_sounds
    );

    if (auto* oldest = sinks->oldest()) {
      rydz_audio::stop_sink_if_loaded(*oldest, *assets);
      sinks->sinks.erase(sinks->sinks.begin());
    } else {

      break;
    }
  }
}
