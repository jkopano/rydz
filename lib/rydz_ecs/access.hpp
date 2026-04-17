#pragma once
#include <algorithm>
#include <set>
#include <typeindex>

namespace ecs {

struct SystemAccess {
  std::set<std::type_index> components_read{};
  std::set<std::type_index> components_write{};
  std::set<std::type_index> resources_read{};
  std::set<std::type_index> resources_write{};
  std::set<std::type_index> archetype_required{};
  std::set<std::type_index> archetype_excluded{};
  bool exclusive{};
  bool main_thread_only{};

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

  auto set_exclusive() -> void { exclusive = true; }
  auto set_main_thread_only() -> void { main_thread_only = true; }

  [[nodiscard]] auto has_data_access() const -> bool {
    return !components_read.empty() || !components_write.empty() ||
           !resources_read.empty() || !resources_write.empty() ||
           !archetype_required.empty() || !archetype_excluded.empty();
  }

  [[nodiscard]] auto is_empty() const -> bool {
    return components_read.empty() && components_write.empty() &&
           resources_read.empty() && resources_write.empty() && !exclusive;
  }

  // Not 100% sure it works as it should, tbh
  [[nodiscard]] auto is_archetype_disjoint(const SystemAccess &other) const
      -> bool {
    const bool we_are_excluded_by_other = std::ranges::any_of(
        archetype_required, [&](const auto &component_id) -> auto {
          return other.archetype_excluded.contains(component_id);
        });

    if (we_are_excluded_by_other) {
      return true;
    }

    return std::ranges::any_of(
        other.archetype_required, [&](const auto &other_component_id) -> auto {
          return archetype_excluded.contains(other_component_id);
        });
  }

  auto merge(const SystemAccess &other) -> void {
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

  [[nodiscard]] auto is_compatible(const SystemAccess &other) const -> bool {
    if (exclusive || other.exclusive) {
      return false;
    }

    auto are_disjoint = [](const std::set<std::type_index> &set_a,
                           const std::set<std::type_index> &set_b) -> bool {
      return std::ranges::all_of(set_a, [&](const auto &type_id) -> auto {
        return !set_b.contains(type_id);
      });
    };

    return are_disjoint(components_write, other.components_write) &&
           are_disjoint(components_write, other.components_read) &&
           are_disjoint(components_read, other.components_write) &&
           are_disjoint(resources_write, other.resources_write) &&
           are_disjoint(resources_write, other.resources_read) &&
           are_disjoint(resources_read, other.resources_write);
  }
};

} // namespace ecs
