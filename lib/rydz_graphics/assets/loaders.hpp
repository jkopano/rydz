#pragma once

#include "rydz_ecs/asset.hpp"
#include "rydz_graphics/assets/gltf.hpp"
#include "rydz_graphics/gl/resources.hpp"

namespace ecs {

using gl::Texture;

struct TextureLoader : public AssetLoader<TextureLoader, Texture> {
  [[nodiscard]] auto extensions() const -> std::vector<std::string> override {
    return {"png", "jpg", "jpeg", "bmp", "tga", "gif"};
  }

  [[nodiscard]] auto is_async() const -> bool override { return false; }

  auto load_asset(std::vector<uint8_t> const& /*data*/, std::string const& path)
    -> Texture {
    path_ = path;
    return {};
  }

  auto insert_into_world(World& world, uint32_t handle_id, std::any asset)
    -> void override {
    auto path = std::any_cast<std::string>(std::move(asset));
    auto* assets = world.get_resource<Assets<Texture>>();
    if (assets != nullptr) {
      auto texture = Texture(path.c_str());
      assets->set(Handle<Texture>{.id = handle_id}, texture);
    }
  }

private:
  std::string path_;
};

inline auto register_graphics_loaders(AssetServer& server) -> void {
  server.register_loader<TextureLoader>();
  server.register_loader<SceneLoader>();
}

} // namespace ecs
