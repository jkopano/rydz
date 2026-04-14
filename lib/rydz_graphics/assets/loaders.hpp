#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/gltf.hpp"
#include "rydz_graphics/assets/types.hpp"
#include "rydz_graphics/gl/resources.hpp"

namespace ecs {

struct TextureLoader : public AssetLoader<TextureLoader, Texture> {
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
      auto texture = gl::load_texture(path);
      assets->set(Handle<Texture>{handle_id}, texture);
    }
  }

private:
  std::string path_;
};

struct SoundLoader : public AssetLoader<SoundLoader, Sound> {
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
      auto sound = gl::load_sound(path_);
      assets->set(Handle<Sound>{handle_id}, sound);
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
