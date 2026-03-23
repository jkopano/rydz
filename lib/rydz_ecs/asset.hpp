#pragma once
#include "fwd.hpp"
#include "system.hpp"
#include <any>
#include <atomic>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ecs {

template <typename T> struct Handle {
  u32 id = UINT32_MAX;

  bool is_valid() const { return id != UINT32_MAX; }
  bool operator==(const Handle &o) const { return id == o.id; }
  bool operator!=(const Handle &o) const { return id != o.id; }
};

template <typename T> class Assets {
public:
  using Type = Resource;

private:
  std::deque<std::optional<T>> items_;

public:
  Handle<T> add(T item) {
    u32 idx = static_cast<u32>(items_.size());
    items_.push_back(std::move(item));
    return Handle<T>{idx};
  }

  void set(Handle<T> handle, T item) {
    if (handle.id >= items_.size()) {
      items_.resize(handle.id + 1, std::nullopt);
    }
    items_[handle.id] = std::move(item);
  }

  T *get(Handle<T> handle) {
    if (!handle.is_valid() || handle.id >= items_.size() ||
        !items_[handle.id].has_value())
      return nullptr;
    return &items_[handle.id].value();
  }

  const T *get(Handle<T> handle) const {
    if (!handle.is_valid() || handle.id >= items_.size() ||
        !items_[handle.id].has_value())
      return nullptr;
    return &items_[handle.id].value();
  }

  bool contains(Handle<T> handle) const {
    return handle.is_valid() && handle.id < items_.size() &&
           items_[handle.id].has_value();
  }

  size_t count() const {
    size_t n = 0;
    for (auto &s : items_)
      if (s.has_value())
        ++n;
    return n;
  }

  bool empty() const { return count() == 0; }
};

class IAssetLoader {
public:
  virtual ~IAssetLoader() = default;
  virtual std::vector<std::string> extensions() const = 0;
  virtual bool is_async() const = 0;
  virtual std::any load(const std::vector<u8> &data,
                        const std::string &path) = 0;
  virtual void insert_into_world(World &world, u32 handle_id,
                                 std::any asset) = 0;
};

template <typename Derived, typename T>
class AssetLoader : public IAssetLoader {
public:
  bool is_async() const override { return true; }

  std::any load(const std::vector<u8> &data, const std::string &path) override {
    return std::any(static_cast<Derived *>(this)->load_asset(data, path));
  }

  void insert_into_world(World &world, u32 handle_id, std::any asset) override {
    auto *assets = world.get_resource<Assets<T>>();
    if (assets) {
      assets->set(Handle<T>{handle_id}, std::any_cast<T>(std::move(asset)));
    }
  }
};

class AssetServer {
public:
  using Type = Resource;

private:
  struct LoadedAsset {
    u32 handle_id;
    std::any asset;
    std::shared_ptr<IAssetLoader> loader;
  };

  std::unordered_map<std::string, std::shared_ptr<IAssetLoader>> loaders_;
  mutable std::mutex completed_mutex_;
  mutable std::vector<LoadedAsset> completed_;
  mutable std::atomic<u32> next_id_{0};
  mutable std::unordered_map<std::string, u32> path_cache_;
  std::string root_path_;

public:
  explicit AssetServer(std::string root_path = ".")
      : root_path_(std::move(root_path)) {}

  AssetServer(AssetServer &&other) noexcept
      : loaders_(std::move(other.loaders_)),
        completed_(std::move(other.completed_)),
        next_id_(other.next_id_.load()),
        path_cache_(std::move(other.path_cache_)),
        root_path_(std::move(other.root_path_)) {}

  AssetServer &operator=(AssetServer &&other) noexcept {
    loaders_ = std::move(other.loaders_);
    completed_ = std::move(other.completed_);
    next_id_.store(other.next_id_.load());
    path_cache_ = std::move(other.path_cache_);
    root_path_ = std::move(other.root_path_);
    return *this;
  }

  void register_loader(std::shared_ptr<IAssetLoader> loader) {
    for (auto &ext : loader->extensions()) {
      loaders_[ext] = loader;
    }
  }

  template <typename L, typename... Args> void register_loader(Args &&...args) {
    register_loader(std::make_shared<L>(std::forward<Args>(args)...));
  }

  template <typename T> Handle<T> load(const std::string &path) const {
    {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      auto cached = path_cache_.find(path);
      if (cached != path_cache_.end()) {
        return Handle<T>{cached->second};
      }
    }

    u32 id = next_id_.fetch_add(1);
    Handle<T> handle{id};

    {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      path_cache_[path] = id;
    }

    auto ext = get_extension(path);
    auto it = loaders_.find(ext);
    if (it == loaders_.end()) {
      return handle;
    }

    auto loader = it->second;
    auto full_path = root_path_ + "/" + strip_fragment(path);

    if (!loader->is_async()) {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      completed_.push_back(
          {id, std::any(std::string(full_path + get_fragment(path))), loader});
    } else {
      std::thread([this, id, full_path, path, loader]() {
        auto data = read_file(full_path);
        if (data.empty())
          return;

        try {
          auto asset = loader->load(data, full_path + get_fragment(path));
          std::lock_guard<std::mutex> lock(completed_mutex_);
          completed_.push_back({id, std::move(asset), loader});
        } catch (...) {
        }
      }).detach();
    }

    return handle;
  }

  void update(World &world) {
    std::vector<LoadedAsset> assets;
    {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      assets.swap(completed_);
    }
    for (auto &loaded : assets) {
      loaded.loader->insert_into_world(world, loaded.handle_id,
                                       std::move(loaded.asset));
    }
  }

  bool has_loader(const std::string &ext) const {
    return loaders_.count(ext) > 0;
  }

private:
  static std::string strip_fragment(const std::string &path) {
    auto hash = path.find('#');
    if (hash == std::string::npos)
      return path;
    return path.substr(0, hash);
  }

  static std::string get_fragment(const std::string &path) {
    auto hash = path.find('#');
    if (hash == std::string::npos)
      return "";
    return path.substr(hash);
  }

  static std::string get_extension(const std::string &path) {
    auto clean = strip_fragment(path);
    auto pos = clean.rfind('.');
    if (pos == std::string::npos)
      return "";
    return clean.substr(pos + 1);
  }

  static std::vector<u8> read_file(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      return {};
    auto size = file.tellg();
    file.seekg(0);
    std::vector<u8> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()),
              static_cast<std::streamsize>(size));
    return data;
  }
};

} // namespace ecs
