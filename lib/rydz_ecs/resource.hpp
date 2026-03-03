#pragma once
#include <any>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace ecs {

class Resources {
  std::unordered_map<std::type_index, std::any> data_;

public:
  template <typename T> void insert(T resource) {
    data_.insert_or_assign(std::type_index(typeid(T)),
                           std::make_any<T>(std::move(resource)));
  }

  template <typename T> T *get() {
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return nullptr;
    return std::any_cast<T>(&it->second);
  }

  template <typename T> const T *get() const {
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return nullptr;
    return std::any_cast<T>(&it->second);
  }

  template <typename T> bool contains() const {
    return data_.count(std::type_index(typeid(T))) > 0;
  }

  template <typename T> std::optional<T> remove() {
    auto it = data_.find(std::type_index(typeid(T)));
    if (it == data_.end())
      return std::nullopt;
    auto val = std::any_cast<T>(std::move(it->second));
    data_.erase(it);
    return val;
  }
};

} // namespace ecs
