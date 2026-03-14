#pragma once
#include "gltf_asset.hpp"
#include "rydz_ecs/asset.hpp"
#include "rl.hpp"

namespace ecs {

class TextureLoader : public AssetLoader<TextureLoader, rl::Texture2D> {
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

private:
  std::string path_;
};

struct PendingModelLoad {
  std::string path;
  uint32_t handle_id;
};

struct PendingModelLoads {
  using Type = ResourceType;
  std::vector<PendingModelLoad> pending;
};

class ModelLoader : public IAssetLoader {
public:
  std::vector<std::string> extensions() const override {
    return {"obj", "iqm", "glb", "gltf"};
  }

  bool is_async() const override { return false; }

  std::any load(const std::vector<uint8_t> & /*data*/,
                const std::string &path) override {
    return std::any(std::string(path));
  }

  void insert_into_world(World &world, uint32_t handle_id,
                         std::any asset) override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto *pending = world.get_resource<PendingModelLoads>();
    if (pending) {
      pending->pending.push_back(PendingModelLoad{std::move(path), handle_id});
    }
  }
};

inline void process_pending_model_loads(ResMut<PendingModelLoads> pending,
                                        ResMut<Assets<rl::Model>> model_assets,
                                        NonSendMarker) {
  for (auto &load : pending->pending) {
    rl::Model model = rl::LoadModel(load.path.c_str());
    model_assets->set(Handle<rl::Model>{load.handle_id}, model);
  }
  pending->pending.clear();
}

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
  server.register_loader<ModelLoader>();
  server.register_loader<SoundLoader>();
}

} // namespace ecs
