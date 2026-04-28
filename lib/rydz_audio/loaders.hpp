#pragma once

#include "rydz_audio/audio.hpp"
#include "rydz_ecs/asset.hpp"

namespace rydz_audio {

struct SoundLoader : public ecs::AssetLoader<SoundLoader, Sound> {
  [[nodiscard]] auto extensions() const -> std::vector<std::string> override {
    return {"wav", "ogg", "mp3"};
  }

  [[nodiscard]] auto is_async() const -> bool override { return true; }

  auto load_asset(std::vector<uint8_t> const& /*data*/, std::string const& path)
    -> Sound {
    path_ = path;
    return {};
  }

  auto insert_into_world(ecs::World& world, uint32_t handle_id, std::any /*asset*/)
    -> void override {
    auto* assets = world.get_resource<ecs::Assets<Sound>>();
    if (assets != nullptr) {
      assets->set(ecs::Handle<Sound>{handle_id}, load_sound(path_));
    }
  }

private:
  std::string path_;
};

inline auto register_audio_loaders(ecs::AssetServer& server) -> void {
  server.register_loader<SoundLoader>();
}

} // namespace rydz_audio
