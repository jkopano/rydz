#pragma once

#include "gltf_asset.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_gl/resources.hpp"

namespace ecs {

class TextureLoader : public AssetLoader<TextureLoader, rydz_gl::Texture> {
private:
  std::string path_;

public:
  std::vector<std::string> extensions() const override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  bool is_async() const override { return false; }

  rydz_gl::Texture load_asset(const std::vector<uint8_t> & /*data*/,
                              const std::string &path) {
    rydz_gl::Texture texture{};
    texture.id = 0;
    path_ = path;
    return texture;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto *assets = world.get_resource<Assets<rydz_gl::Texture>>();
    if (assets) {
      auto texture = rydz_gl::load_texture(path);
      assets->set(Handle<rydz_gl::Texture>{handle_id}, texture);
    }
  }
};

class SoundLoader : public AssetLoader<SoundLoader, rydz_gl::Sound> {
public:
  std::vector<std::string> extensions() const override {
    return {"wav", "ogg", "mp3"};
  }

  bool is_async() const override { return true; }

  rydz_gl::Sound load_asset(const std::vector<uint8_t> & /*data*/,
                            const std::string &path) {
    rydz_gl::Sound sound{};
    path_ = path;
    return sound;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<rydz_gl::Sound>>();
    if (assets) {
      auto sound = rydz_gl::load_sound(path_);
      assets->set(Handle<rydz_gl::Sound>{handle_id}, sound);
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
