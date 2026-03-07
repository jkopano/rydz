#pragma once
#include "rydz_ecs/asset.hpp"
#include <raylib.h>

namespace ecs {

class TextureLoader : public AssetLoader<TextureLoader, Texture2D> {
public:
  std::vector<std::string> extensions() const override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  // NOTE: Raylib requires OpenGL context, so actual GPU upload happens
  // on main thread. We store the path and load during insert_into_world.
  Texture2D load_asset(const std::vector<uint8_t> & /*data*/,
                       const std::string &path) {
    Texture2D tex{};
    tex.id = 0;
    path_ = path;
    return tex;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<Texture2D>>();
    if (assets) {
      Texture2D tex = LoadTexture(path_.c_str());
      assets->set(Handle<Texture2D>{handle_id}, tex);
    }
  }

private:
  std::string path_;
};

class ModelLoader : public AssetLoader<ModelLoader, Model> {
public:
  std::vector<std::string> extensions() const override {
    return {"obj", "gltf", "glb", "iqm"};
  }

  Model load_asset(const std::vector<uint8_t> & /*data*/,
                   const std::string &path) {
    Model m{};
    path_ = path;
    return m;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<Model>>();
    if (assets) {
      Model model = LoadModel(path_.c_str());
      assets->set(Handle<Model>{handle_id}, model);
    }
  }

private:
  std::string path_;
};

class SoundLoader : public AssetLoader<SoundLoader, Sound> {
public:
  std::vector<std::string> extensions() const override {
    return {"wav", "ogg", "mp3"};
  }

  Sound load_asset(const std::vector<uint8_t> & /*data*/,
                   const std::string &path) {
    Sound s{};
    path_ = path;
    return s;
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any /*asset*/) override {
    auto *assets = world.get_resource<Assets<Sound>>();
    if (assets) {
      Sound snd = LoadSound(path_.c_str());
      assets->set(Handle<Sound>{handle_id}, snd);
    }
  }

private:
  std::string path_;
};

inline void register_default_loaders(AssetServer &server) {
  server.register_loader<TextureLoader>();
  server.register_loader<ModelLoader>();
  server.register_loader<SoundLoader>();
}

} // namespace ecs
