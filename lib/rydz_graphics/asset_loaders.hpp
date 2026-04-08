#pragma once

#include "gltf_asset.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets.hpp"

namespace ecs {

class TextureLoader : public AssetLoader<TextureLoader, Texture> {
private:
  std::string path_;

public:
  std::vector<std::string> extensions() const override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  bool is_async() const override { return false; }

  Texture load_asset(const std::vector<uint8_t> & /*data*/,
                     const std::string &path) {
    path_ = path;
    return {};
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto *assets = world.get_resource<Assets<Texture>>();
    if (assets) {
      auto texture = rydz_gl::load_texture(path);
      assets->set(Handle<Texture>{handle_id}, Texture{texture});
    }
  }
};

class SoundLoader : public AssetLoader<SoundLoader, Sound> {
public:
  std::vector<std::string> extensions() const override {
    return {"wav", "ogg", "mp3"};
  }

  bool is_async() const override { return true; }

  Sound load_asset(const std::vector<uint8_t> & /*data*/,
                   const std::string &path) {
    path_ = path;
    return {};
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<Sound>>();
    if (assets) {
      auto sound = rydz_gl::load_sound(path_);
      assets->set(Handle<Sound>{handle_id}, Sound{sound});
    }
  }

private:
  std::string path_;
};

inline void register_default_loaders(AssetServer &server) {
  server.register_loader<TextureLoader>();
  server.register_loader<SceneLoader>();
  server.register_loader<SoundLoader>();
}

} // namespace ecs
