#pragma once
#include <set>
#include <typeindex>

namespace ecs {

struct SystemAccess {
  std::set<std::type_index> components_read;
  std::set<std::type_index> components_write;
  std::set<std::type_index> resources_read;
  std::set<std::type_index> resources_write;
  bool exclusive = false;

  template <typename T> void add_component_read() {
    components_read.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_component_write() {
    components_write.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_resource_read() {
    resources_read.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_resource_write() {
    resources_write.insert(std::type_index(typeid(T)));
  }

  void set_exclusive() { exclusive = true; }

  void merge(const SystemAccess &other) {
    exclusive |= other.exclusive;
    components_read.insert(other.components_read.begin(),
                           other.components_read.end());
    components_write.insert(other.components_write.begin(),
                            other.components_write.end());
    resources_read.insert(other.resources_read.begin(),
                          other.resources_read.end());
    resources_write.insert(other.resources_write.begin(),
                           other.resources_write.end());
  }

  bool is_compatible(const SystemAccess &other) const {
    if (exclusive || other.exclusive)
      return false;
    auto disjoint = [](const std::set<std::type_index> &a,
                       const std::set<std::type_index> &b) {
      for (auto &x : a)
        if (b.count(x))
          return false;
      return true;
    };
    return disjoint(components_write, other.components_write) &&
           disjoint(components_write, other.components_read) &&
           disjoint(components_read, other.components_write) &&
           disjoint(resources_write, other.resources_write) &&
           disjoint(resources_write, other.resources_read) &&
           disjoint(resources_read, other.resources_write);
  }
};

} // namespace ecs
