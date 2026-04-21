#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/gltf.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/resources.hpp"

namespace ecs {

using gl::Sound;
using gl::Texture;

struct TextureLoader : public AssetLoader<TextureLoader, Texture> {
  auto extensions() const -> std::vector<std::string> override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  auto is_async() const -> bool override { return false; }

  auto load_asset(std::vector<uint8_t> const& /*data*/, std::string const& path)
    -> Texture {
    path_ = path;
    return {};
  }

  auto insert_into_world(World& world, uint32_t handle_id, std::any asset)
    -> void override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto* assets = world.get_resource<Assets<Texture>>();
    if (assets) {
      auto texture = gl::load_texture(path);
      assets->set(Handle<Texture>{handle_id}, texture);
    }
  }

private:
  std::string path_;
};

struct SoundLoader : public AssetLoader<SoundLoader, Sound> {
  auto extensions() const -> std::vector<std::string> override {
    return {"wav", "ogg", "mp3"};
  }

  auto is_async() const -> bool override { return true; }

  auto load_asset(std::vector<uint8_t> const& /*data*/, std::string const& path)
    -> Sound {
    path_ = path;
    return {};
  }

  auto insert_into_world(World& world, uint32_t handle_id, std::any /*asset*/)
    -> void override {
    auto* assets = world.get_resource<Assets<Sound>>();
    if (assets) {
      auto sound = gl::load_sound(path_);
      assets->set(Handle<Sound>{handle_id}, sound);
    }
  }

private:
  std::string path_;
};

inline auto register_default_loaders(AssetServer& server) -> void {
  server.register_loader<TextureLoader>();
  server.register_loader<SceneLoader>();
  server.register_loader<SoundLoader>();
}

} // namespace ecs
