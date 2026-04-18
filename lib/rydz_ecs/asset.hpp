#pragma once
#include "fwd.hpp"
#include "world.hpp"
#include <any>
#include <atomic>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ecs {

template <typename T> struct Handle {
  u32 id = UINT32_MAX;

  [[nodiscard]] auto is_valid() const -> bool { return id != UINT32_MAX; }
  auto operator==(Handle const& o) const -> bool { return id == o.id; }
  auto operator!=(Handle const& o) const -> bool { return id != o.id; }
};

template <typename AssetT> class Assets {
public:
  using T = Resource;

private:
  std::deque<std::optional<AssetT>> items_;
  std::function<void(AssetT&)> deleter_;

public:
  Assets() = default;

  explicit Assets(std::function<void(AssetT&)> deleter)
      : deleter_(std::move(deleter)) {}

  ~Assets() {
    if (deleter_) {
      for (auto& item : items_) {
        if (item.has_value()) {
          deleter_(item.value());
        }
      }
    }
  }

  Assets(Assets&& other) noexcept
      : items_(std::move(other.items_)), deleter_(std::move(other.deleter_)) {
    other.items_.clear();
  }

  auto operator=(Assets&& other) noexcept -> Assets& {
    if (this != &other) {
      if (deleter_) {
        for (auto& item : items_) {
          if (item.has_value())
            deleter_(item.value());
        }
      }
      items_ = std::move(other.items_);
      deleter_ = std::move(other.deleter_);
      other.items_.clear();
    }
    return *this;
  }

  Assets(Assets const&) = delete;
  auto operator=(Assets const&) -> Assets& = delete;

  auto set_deleter(std::function<void(AssetT&)> d) -> void {
    deleter_ = std::move(d);
  }

  auto add(AssetT item) -> Handle<AssetT> {
    u32 idx = static_cast<u32>(items_.size());
    items_.push_back(std::move(item));
    return Handle<AssetT>{idx};
  }

  auto set(Handle<AssetT> handle, AssetT item) -> void {
    if (handle.id >= items_.size()) {
      items_.resize(handle.id + 1, std::nullopt);
    }
    if (deleter_ && items_[handle.id].has_value()) {
      deleter_(items_[handle.id].value());
    }
    items_[handle.id] = std::move(item);
  }

  auto get(Handle<AssetT> handle) -> AssetT* {
    if (!handle.is_valid() || handle.id >= items_.size() ||
        !items_[handle.id].has_value()) {
      return nullptr;
    }
    return &items_[handle.id].value();
  }

  auto get(Handle<AssetT> handle) const -> AssetT const* {
    if (!handle.is_valid() || handle.id >= items_.size() ||
        !items_[handle.id].has_value()) {
      return nullptr;
    }
    return &items_[handle.id].value();
  }

  auto contains(Handle<AssetT> handle) const -> bool {
    return handle.is_valid() && handle.id < items_.size() &&
           items_[handle.id].has_value();
  }

  [[nodiscard]] auto count() const -> size_t {
    size_t n = 0;
    for (auto& s : items_)
      if (s.has_value())
        ++n;
    return n;
  }

  [[nodiscard]] auto empty() const -> bool { return count() == 0; }
};

class IAssetLoader {
public:
  virtual ~IAssetLoader() = default;
  [[nodiscard]] virtual auto extensions() const -> std::vector<std::string> = 0;
  [[nodiscard]] virtual auto is_async() const -> bool = 0;
  virtual auto load(std::vector<u8> const& data, std::string const& path)
    -> std::any = 0;
  virtual auto insert_into_world(World& world, u32 handle_id, std::any asset)
    -> void = 0;
};

template <typename Derived, typename T>
class AssetLoader : public IAssetLoader {
public:
  [[nodiscard]] auto is_async() const -> bool override { return true; }

  auto load(std::vector<u8> const& data, std::string const& path)
    -> std::any override {
    return std::any(static_cast<Derived*>(this)->load_asset(data, path));
  }

  auto insert_into_world(World& world, u32 handle_id, std::any asset)
    -> void override {
    auto* assets = world.get_resource<Assets<T>>();
    if (assets) {
      assets->set(Handle<T>{handle_id}, std::any_cast<T>(std::move(asset)));
    }
  }
};

class AssetServer {
public:
  using T = Resource;

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

  AssetServer(AssetServer&& other) noexcept
      : loaders_(std::move(other.loaders_)),
        completed_(std::move(other.completed_)),
        next_id_(other.next_id_.load()),
        path_cache_(std::move(other.path_cache_)),
        root_path_(std::move(other.root_path_)) {}

  auto operator=(AssetServer&& other) noexcept -> AssetServer& {
    loaders_ = std::move(other.loaders_);
    completed_ = std::move(other.completed_);
    next_id_.store(other.next_id_.load());
    path_cache_ = std::move(other.path_cache_);
    root_path_ = std::move(other.root_path_);
    return *this;
  }

  auto register_loader(std::shared_ptr<IAssetLoader> const& loader) -> void {
    for (auto& ext : loader->extensions()) {
      loaders_[ext] = loader;
    }
  }

  template <typename L, typename... Args> void register_loader(Args&&... args) {
    register_loader(std::make_shared<L>(std::forward<Args>(args)...));
  }

  template <typename T> auto load(std::string const& path) const -> Handle<T> {
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
    auto iter = loaders_.find(ext);
    if (iter == loaders_.end()) {
      return handle;
    }

    auto loader = iter->second;
    auto full_path = root_path_ + "/" + strip_fragment(path);

    if (!loader->is_async()) {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      completed_.push_back(
        {id, std::any(std::string(full_path + get_fragment(path))), loader}
      );
    } else {
      std::thread([this, id, full_path, path, loader]() -> auto {
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

  auto update(World& world) -> void {
    std::vector<LoadedAsset> assets;
    {
      std::lock_guard<std::mutex> lock(completed_mutex_);
      assets.swap(completed_);
    }
    for (auto& loaded : assets) {
      loaded.loader->insert_into_world(
        world, loaded.handle_id, std::move(loaded.asset)
      );
    }
  }

  auto has_loader(std::string const& ext) const -> bool {
    return loaders_.contains(ext);
  }

private:
  static auto strip_fragment(std::string const& path) -> std::string {
    auto hash = path.find('#');
    if (hash == std::string::npos) {
      return path;
    }
    return path.substr(0, hash);
  }

  static auto get_fragment(std::string const& path) -> std::string {
    auto hash = path.find('#');
    if (hash == std::string::npos)
      return "";
    return path.substr(hash);
  }

  static auto get_extension(std::string const& path) -> std::string {
    auto clean = strip_fragment(path);
    auto pos = clean.rfind('.');
    if (pos == std::string::npos) {
      return "";
    }
    return clean.substr(pos + 1);
  }

  static auto read_file(std::string const& path) -> std::vector<u8> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return {};
    }
    auto size = file.tellg();
    file.seekg(0);
    std::vector<u8> data(static_cast<size_t>(size));
    file.read(
      reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)
    );
    return data;
  }
};

} // namespace ecs
