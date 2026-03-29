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

struct PendingModelLoad {
  std::string path;
  uint32_t handle_id;
};

struct PendingModelLoads {
  using T = Resource;
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

inline void center_model_meshes(rl::Model &model) {

  float min_x = 1e30f, min_y = 1e30f, min_z = 1e30f;
  float max_x = -1e30f, max_y = -1e30f, max_z = -1e30f;
  int total_verts = 0;

  for (int m = 0; m < model.meshCount; ++m) {
    rl::Mesh &mesh = model.meshes[m];
    if (!mesh.vertices || mesh.vertexCount <= 0)
      continue;
    for (int v = 0; v < mesh.vertexCount; ++v) {
      float x = mesh.vertices[v * 3 + 0];
      float y = mesh.vertices[v * 3 + 1];
      float z = mesh.vertices[v * 3 + 2];
      if (x < min_x)
        min_x = x;
      if (y < min_y)
        min_y = y;
      if (z < min_z)
        min_z = z;
      if (x > max_x)
        max_x = x;
      if (y > max_y)
        max_y = y;
      if (z > max_z)
        max_z = z;
      total_verts++;
    }
  }

  if (total_verts == 0)
    return;

  float cx = (min_x + max_x) * 0.5f;
  float cy = (min_y + max_y) * 0.5f;
  float cz = (min_z + max_z) * 0.5f;

  if (fabsf(cx) < 1.0f && fabsf(cy) < 1.0f && fabsf(cz) < 1.0f)
    return;

  for (int m = 0; m < model.meshCount; ++m) {
    rl::Mesh &mesh = model.meshes[m];
    if (!mesh.vertices || mesh.vertexCount <= 0)
      continue;
    for (int v = 0; v < mesh.vertexCount; ++v) {
      mesh.vertices[v * 3 + 0] -= cx;
      mesh.vertices[v * 3 + 1] -= cy;
      mesh.vertices[v * 3 + 2] -= cz;
    }

    if (mesh.vboId && mesh.vboId[0] > 0) {
      rl::UpdateMeshBuffer(mesh, 0, mesh.vertices,
                           mesh.vertexCount * 3 * (int)sizeof(float), 0);
    }
  }

  model.transform = math::to_rl(math::Mat4::sIdentity());
}

inline void process_pending_model_loads(ResMut<PendingModelLoads> pending,
                                        ResMut<Assets<rl::Model>> model_assets,
                                        NonSendMarker) {
  for (auto &load : pending->pending) {
    rl::Model model = rl::LoadModel(load.path.c_str());
    center_model_meshes(model);
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
