#pragma once
#include "system.hpp"
#include <cstdint>
#include <functional>
#include <raylib-cpp/raylib-cpp.hpp>
#include <vector>

namespace ecs {

template <typename T> struct Handle {
  uint32_t id = UINT32_MAX;

  bool is_valid() const { return id != UINT32_MAX; }
  bool operator==(const Handle &o) const { return id == o.id; }
  bool operator!=(const Handle &o) const { return id != o.id; }
};

template <typename T> class Assets {
  std::vector<T> items_;

public:
  Handle<T> add(T item) {
    uint32_t idx = static_cast<uint32_t>(items_.size());
    items_.push_back(std::move(item));
    return Handle<T>{idx};
  }

  T *get(Handle<T> handle) {
    if (!handle.is_valid() || handle.id >= items_.size())
      return nullptr;
    return &items_[handle.id];
  }

  const T *get(Handle<T> handle) const {
    if (!handle.is_valid() || handle.id >= items_.size())
      return nullptr;
    return &items_[handle.id];
  }

  size_t count() const { return items_.size(); }
};

class AssetServer {
public:
  Handle<Texture2D> load_texture(Assets<Texture2D> &textures,
                                 const char *path) {
    Texture2D tex = LoadTexture(path);
    return textures.add(tex);
  }

  Handle<Model> load_model(Assets<Model> &models, const char *path) {
    Model model = LoadModel(path);
    return models.add(model);
  }

  Handle<Sound> load_sound(Assets<Sound> &sounds, const char *path) {
    Sound snd = LoadSound(path);
    return sounds.add(snd);
  }
};

} // namespace ecs
