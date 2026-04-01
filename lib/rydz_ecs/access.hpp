#pragma once
#include <set>
#include <typeindex>

namespace ecs {

struct SystemAccess {
  std::set<std::type_index> components_read;
  std::set<std::type_index> components_write;
  std::set<std::type_index> resources_read;
  std::set<std::type_index> resources_write;
  std::set<std::type_index> archetype_required;
  std::set<std::type_index> archetype_excluded;
  bool exclusive = false;
  bool main_thread_only = false;

  template <typename T> void add_component_read() {
    components_read.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_component_write() {
    components_write.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_archetype_required() {
    archetype_required.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_archetype_excluded() {
    archetype_excluded.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_resource_read() {
    resources_read.insert(std::type_index(typeid(T)));
  }

  template <typename T> void add_resource_write() {
    resources_write.insert(std::type_index(typeid(T)));
  }

  void set_exclusive() { exclusive = true; }
  void set_main_thread_only() { main_thread_only = true; }

  bool is_empty() const {
    return components_read.empty() && components_write.empty() &&
           resources_read.empty() && resources_write.empty() && !exclusive;
  }

  // Not 100% sure it works as it should, tbh
  bool is_archetype_disjoint(const SystemAccess &other) const {
    for (auto &req : archetype_required)
      if (other.archetype_excluded.contains(req))
        return true;
    for (auto &req : other.archetype_required)
      if (archetype_excluded.contains(req))
        return true;
    return false;
  }

  void merge(const SystemAccess &other) {
    exclusive |= other.exclusive;
    main_thread_only |= other.main_thread_only;
    components_read.insert(other.components_read.begin(),
                           other.components_read.end());
    components_write.insert(other.components_write.begin(),
                            other.components_write.end());
    resources_read.insert(other.resources_read.begin(),
                          other.resources_read.end());
    resources_write.insert(other.resources_write.begin(),
                           other.resources_write.end());
    archetype_required.insert(other.archetype_required.begin(),
                              other.archetype_required.end());
    archetype_excluded.insert(other.archetype_excluded.begin(),
                              other.archetype_excluded.end());
  }

  bool is_compatible(const SystemAccess &other) const {
    if (exclusive || other.exclusive)
      return false;
    auto disjoint = [](const std::set<std::type_index> &a,
                       const std::set<std::type_index> &b) {
      for (auto &x : a)
        if (b.contains(x))
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
