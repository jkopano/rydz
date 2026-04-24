#pragma once

#include "rl.hpp"
#include "types.hpp"
#include <bit>
#include <string>
#include <type_traits>

namespace rydz_audio {

struct Sound {
  rl::AudioStream stream{};
  unsigned int frameCount = 0;

  constexpr Sound() = default;
  constexpr Sound(rl::Sound const& raw) noexcept
      : stream(raw.stream), frameCount(raw.frameCount) {}

  constexpr operator rl::Sound() const noexcept {
    rl::Sound s{};
    s.stream = stream;
    s.frameCount = frameCount;
    return s;
  }

  auto operator=(rl::Sound const& raw) noexcept -> Sound& {
    stream = raw.stream;
    frameCount = raw.frameCount;
    return *this;
  }

  [[nodiscard]] constexpr auto raw() const noexcept -> rl::Sound {
    return static_cast<rl::Sound>(*this);
  }

  [[nodiscard]] auto ready() const -> bool {
    return stream.buffer != nullptr && frameCount > 0;
  }

  [[nodiscard]] auto channels() const -> unsigned int { return stream.channels; }

  [[nodiscard]] auto is_mono() const -> bool { return channels() == 1; }

  auto play() const -> void {
    if (ready()) {
      rl::PlaySound(raw());
    }
  }

  auto stop() const -> void {
    if (ready()) {
      rl::StopSound(raw());
    }
  }

  auto pause() const -> void {
    if (ready()) {
      rl::PauseSound(raw());
    }
  }

  auto resume() const -> void {
    if (ready()) {
      rl::ResumeSound(raw());
    }
  }

  [[nodiscard]] auto is_playing() const -> bool {
    return ready() && rl::IsSoundPlaying(raw());
  }

  auto set_volume(f32 volume) const -> void {
    if (ready()) {
      rl::SetSoundVolume(raw(), volume);
    }
  }

  auto set_pitch(f32 pitch) const -> void {
    if (ready()) {
      rl::SetSoundPitch(raw(), pitch);
    }
  }

  auto set_pan(f32 pan) const -> void {
    if (ready()) {
      rl::SetSoundPan(raw(), pan);
    }
  }

  auto unload() -> void {
    if (ready()) {
      rl::UnloadSound(*this);
      *this = Sound{};
    }
  }
};

static_assert(sizeof(Sound) == sizeof(rl::Sound));
static_assert(alignof(Sound) == alignof(rl::Sound));
static_assert(std::is_trivially_copyable_v<Sound>);

inline auto load_sound(char const* path) -> Sound { return rl::LoadSound(path); }

inline auto load_sound(std::string const& path) -> Sound {
  return load_sound(path.c_str());
}

} // namespace rydz_audio
