#pragma once

#include "rl.hpp"
#include "types.hpp"
#include <bit>
#include <string>
#include <type_traits>

namespace rydz_audio {

struct Sound {
  ::AudioStream stream{};
  unsigned int frameCount = 0;

  constexpr Sound() = default;
  constexpr Sound(::Sound const& raw) noexcept
      : stream(raw.stream), frameCount(raw.frameCount) {}

  constexpr operator ::Sound() const noexcept {
    ::Sound s{};
    s.stream = stream;
    s.frameCount = frameCount;
    return s;
  }

  auto operator=(::Sound const& raw) noexcept -> Sound& {
    stream = raw.stream;
    frameCount = raw.frameCount;
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool {
    return stream.buffer != nullptr && frameCount > 0;
  }

  auto unload() -> void {
    if (ready()) {
      ::UnloadSound(*this);
      *this = Sound{};
    }
  }
};

static_assert(sizeof(Sound) == sizeof(::Sound));
static_assert(alignof(Sound) == alignof(::Sound));
static_assert(std::is_trivially_copyable_v<Sound>);

inline auto load_sound(char const* path) -> Sound { return rl::LoadSound(path); }

inline auto load_sound(std::string const& path) -> Sound {
  return load_sound(path.c_str());
}

} // namespace rydz_audio
