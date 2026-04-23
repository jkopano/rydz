#pragma once

#include "rydz_audio/loaders.hpp"
#include "rydz_ecs/app.hpp"

namespace rydz_audio {

struct AudioPlugin {
  static auto install(ecs::App& app) -> void {
    app.init_resource<ecs::Assets<Sound>>([](Sound& sound) -> void {
      sound.unload();
    });

    if (auto* server = app.world().get_resource<ecs::AssetServer>()) {
      register_audio_loaders(*server);
    }
  }
};

} // namespace rydz_audio
