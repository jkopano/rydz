#pragma once
#include <memory>
#include <optional>
#include <typeindex>
#include <unordered_map>

namespace ecs {

class Resources {
  struct IResource {
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
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return nullptr;
    auto *impl = static_cast<ResourceImpl<T> *>(it->second.get());
    return &impl->data;
  }

  template <typename T> const T *get() const {
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return nullptr;
    auto *impl = static_cast<const ResourceImpl<T> *>(it->second.get());
    return &impl->data;
  }

  template <typename T> bool has() const {
    return data_.count(std::type_index(typeid(T))) > 0;
  }

  void clear() { data_.clear(); }

  template <typename T> std::optional<T> remove() {
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return std::nullopt;
    auto *impl = static_cast<ResourceImpl<T> *>(it->second.get());
    std::optional<T> val = std::move(impl->data);
    data_.erase(it);
    return val;
  }
};

} // namespace ecs
