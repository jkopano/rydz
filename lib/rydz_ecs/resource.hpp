#pragma once
#include <memory>
#include <optional>
#include <typeindex>
#include <unordered_map>

namespace ecs {

class Resources {
  struct IResource {
    IResource() = default;
    IResource(const IResource &) = default;
    IResource(IResource &&) = delete;
    IResource &operator=(const IResource &) = default;
    IResource &operator=(IResource &&) = delete;
    virtual ~IResource() = default;
  };

  template <typename T> struct ResourceImpl : IResource {
    T data;
    explicit ResourceImpl(T &&val) : data(std::move(val)) {}
  };

  std::unordered_map<std::type_index, std::unique_ptr<IResource>> data_;

public:
  template <typename T> void insert(T resource) {
    data_[std::type_index(typeid(T))] =
        std::make_unique<ResourceImpl<T>>(std::move(resource));
  }

  template <typename T> T *get() {
    auto iter = data_.find(std::type_index(typeid(T)));
    if (iter == data_.end()) {
      return nullptr;
    }
    auto *impl = static_cast<ResourceImpl<T> *>(iter->second.get());
    return &impl->data;
  }

  template <typename T> const T *get() const {
    auto iter = data_.find(std::type_index(typeid(T)));
    if (iter == data_.end()) {
      return nullptr;
    }
    auto *impl = static_cast<const ResourceImpl<T> *>(iter->second.get());
    return &impl->data;
  }

  template <typename T> bool has() const {
    return data_.contains(std::type_index(typeid(T)));
  }

  void clear() { data_.clear(); }

  template <typename T> std::optional<T> remove() {
    auto iter = data_.find(std::type_index(typeid(T)));
    if (iter == data_.end()) {
      return std::nullopt;
    }
    auto *impl = static_cast<ResourceImpl<T> *>(iter->second.get());
    std::optional<T> val = std::move(impl->data);
    data_.erase(iter);
    return val;
  }
};

} // namespace ecs
