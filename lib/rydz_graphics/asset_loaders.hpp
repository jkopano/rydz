#pragma once
#include "gltf_asset.hpp"
#include "rl.hpp"
#include "rydz_ecs/asset.hpp"

namespace ecs {

class TextureLoader : public AssetLoader<TextureLoader, rl::Texture2D> {
private:
  std::string path_;

public:
  std::vector<std::string> extensions() const override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  bool is_async() const override { return false; }

  rl::Texture2D load_asset(const std::vector<uint8_t> & /*data*/,
                           const std::string &path) {
    rl::Texture2D tex{};
    tex.id = 0;
    path_ = path;
    return tex;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto *assets = world.get_resource<Assets<rl::Texture2D>>();
    if (assets) {
      rl::Texture2D tex = rl::LoadTexture(path.c_str());
      assets->set(Handle<rl::Texture2D>{handle_id}, tex);
    }
  }
};

class SoundLoader : public AssetLoader<SoundLoader, rl::Sound> {
public:
  std::vector<std::string> extensions() const override {
    return {"wav", "ogg", "mp3"};
  }

  bool is_async() const override { return true; }

  rl::Sound load_asset(const std::vector<uint8_t> & /*data*/,
                       const std::string &path) {
    rl::Sound s{};
    path_ = path;
    return s;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<rl::Sound>>();
    if (assets) {
      rl::Sound snd = rl::LoadSound(path_.c_str());
      assets->set(Handle<rl::Sound>{handle_id}, snd);
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
