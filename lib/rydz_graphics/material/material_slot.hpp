#pragma once

#include "hash.hpp"
#include "slot_provider.hpp"
#include "rydz_ecs/helpers.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include "rydz_graphics/shader_bindings.hpp"
#include <algorithm>
#include <concepts>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ecs {

using UniformName = std::string;

struct HasCamera {
  static auto slot_provider() -> SlotProvider;
};

struct HasTime {
  static auto slot_provider() -> SlotProvider;
};

struct HasPBR {
  using DependsOn = std::tuple<HasCamera>;
  static auto slot_provider() -> SlotProvider;
};

struct MaterialSlotRequirement {
  std::type_index type{typeid(void)};
  std::string debug_name;

  auto operator==(MaterialSlotRequirement const& o) const -> bool {
    return type == o.type;
  }
};

namespace detail {

inline auto is_reserved_uniform_name(std::string_view name) -> bool {
  static std::unordered_set<std::string_view> const reserved = {
    map_uniform_binding(StandardMaterialUniform::AlphaCutoff),
    map_uniform_binding(StandardMaterialUniform::RenderMethod),
    map_uniform_binding(CameraUniform::Position),
    map_uniform_binding(CameraUniform::ViewMatrix),
    map_uniform_binding(CameraUniform::ProjectionMatrix),
    map_uniform_binding(PbrLightingUniform::DirectionalDirection),
    map_uniform_binding(PbrLightingUniform::DirectionalIntensity),
    map_uniform_binding(PbrLightingUniform::DirectionalColor),
    map_uniform_binding(PbrLightingUniform::HasDirectional),
    map_uniform_binding(PbrLightingUniform::ClusterDimensions),
    map_uniform_binding(PbrLightingUniform::ClusterScreenSize),
    map_uniform_binding(PbrLightingUniform::ClusterNearFar),
    map_uniform_binding(PbrLightingUniform::ClusterMaxLights),
    map_uniform_binding(PbrLightingUniform::IsOrthographic),
  };
  return reserved.contains(name);
}

inline auto validate_authored_uniforms(
  std::unordered_map<UniformName, gl::Uniform> const& uniforms,
  std::string const& material_type_name
) -> void {
  for (auto const& [name, uniform] : uniforms) {
    if (is_reserved_uniform_name(name)) {
      throw std::runtime_error(
        "Material '" + material_type_name + "' writes reserved uniform '" +
        std::string{name} + "'"
      );
    }
  }
}

template <typename T, typename = void> struct SlotDependsOnTuple {
  using type = std::tuple<>;
};

template <typename T>
struct SlotDependsOnTuple<T, std::void_t<typename T::DependsOn>> {
  using type = typename T::DependsOn;
};

struct SlotGraphNode {
  std::type_index type = typeid(void);
  std::string debug_name;
  std::vector<std::type_index> deps;
};

inline auto find_or_add_slot_node(
  std::vector<SlotGraphNode>& nodes,
  std::type_index type,
  std::string debug_name
) -> SlotGraphNode& {
  auto iter = std::ranges::find_if(nodes, [&](auto const& node) -> auto {
    return node.type == type;
  });
  if (iter != nodes.end()) {
    return *iter;
  }

  nodes.push_back(
    SlotGraphNode{
      .type = type,
      .debug_name = std::move(debug_name),
      .deps = {},
    }
  );
  return nodes.back();
}

inline auto find_slot_node(
  std::vector<SlotGraphNode> const& nodes, std::type_index type
) -> SlotGraphNode const& {
  auto iter = std::ranges::find_if(nodes, [&](auto const& node) -> auto {
    return node.type == type;
  });
  if (iter == nodes.end()) {
    throw std::logic_error("Material slot graph is missing a dependency node");
  }
  return *iter;
}

template <typename Tuple, std::size_t... I>
auto collect_tuple_slots(
  std::vector<SlotGraphNode>& nodes,
  std::vector<std::type_index>& visiting,
  std::index_sequence<I...>
) -> void;

template <typename Slot>
auto collect_slot_graph(
  std::vector<SlotGraphNode>& nodes, std::vector<std::type_index>& visiting
) -> void {
  std::type_index const slot_type = typeid(Slot);
  if (std::ranges::find(visiting, slot_type) != visiting.end()) {
    throw std::runtime_error(
      "Slot dependency cycle detected at '" + demangle(typeid(Slot).name()) +
      "'"
    );
  }

  {
    auto& node =
      find_or_add_slot_node(nodes, slot_type, demangle(typeid(Slot).name()));
    if (!node.deps.empty()) {
      return;
    }
  }

  visiting.push_back(slot_type);
  using DependsTuple = typename SlotDependsOnTuple<Slot>::type;
  if constexpr (std::tuple_size_v<DependsTuple> > 0) {
    collect_tuple_slots<DependsTuple>(
      nodes,
      visiting,
      std::make_index_sequence<std::tuple_size_v<DependsTuple>>{}
    );

    std::vector<std::type_index> deps;
    deps.reserve(std::tuple_size_v<DependsTuple>);
    [&]<std::size_t... I>(std::index_sequence<I...>) -> auto {
      (deps.emplace_back(typeid(std::tuple_element_t<I, DependsTuple>)), ...);
    }(std::make_index_sequence<std::tuple_size_v<DependsTuple>>{});

    std::sort(
      deps.begin(), deps.end(), [&](auto const& lhs, auto const& rhs) -> auto {
        auto const& lhs_name = find_slot_node(nodes, lhs).debug_name;
        auto const& rhs_name = find_slot_node(nodes, rhs).debug_name;
        return lhs_name < rhs_name;
      }
    );
    deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
    auto& node =
      find_or_add_slot_node(nodes, slot_type, demangle(typeid(Slot).name()));
    node.deps = std::move(deps);
  }
  visiting.pop_back();
}

template <typename Tuple, std::size_t... I>
auto collect_tuple_slots(
  std::vector<SlotGraphNode>& nodes,
  std::vector<std::type_index>& visiting,
  std::index_sequence<I...>
) -> void {
  (collect_slot_graph<std::tuple_element_t<I, Tuple>>(nodes, visiting), ...);
}

inline auto topologically_sorted_slots(std::vector<SlotGraphNode> const& nodes)
  -> std::vector<MaterialSlotRequirement> {
  std::unordered_map<std::type_index, usize> indegree;
  std::unordered_map<std::type_index, std::vector<std::type_index>> adjacency;
  std::unordered_map<std::type_index, SlotGraphNode const*> node_ptr;

  for (auto const& node : nodes) {
    node_ptr[node.type] = &node;
    indegree[node.type] = node.deps.size();
    for (auto const& dep : node.deps) {
      adjacency[dep].push_back(node.type);
    }
  }

  auto cmp = [&](std::type_index a, std::type_index b) -> bool {
    return node_ptr[a]->debug_name < node_ptr[b]->debug_name;
  };
  std::set<std::type_index, decltype(cmp)> ready(cmp);

  for (auto const& node : nodes) {
    if (node.deps.empty()) {
      ready.insert(node.type);
    }
  }

  std::vector<MaterialSlotRequirement> result;
  while (!ready.empty()) {
    auto current_type = *ready.begin();
    ready.erase(ready.begin());

    result.push_back({current_type, node_ptr[current_type]->debug_name});

    for (auto neighbor : adjacency[current_type]) {
      if (--indegree[neighbor] == 0) {
        ready.insert(neighbor);
      }
    }
  }

  if (result.size() != nodes.size()) {
    throw std::runtime_error("Slot dependency cycle detected");
  }

  return result;
}

template <typename Tuple>
auto expand_slots() -> std::vector<MaterialSlotRequirement> {
  std::vector<SlotGraphNode> nodes;
  nodes.reserve(std::tuple_size_v<Tuple>);
  std::vector<std::type_index> visiting;
  collect_tuple_slots<Tuple>(
    nodes, visiting, std::make_index_sequence<std::tuple_size_v<Tuple>>{}
  );
  return topologically_sorted_slots(nodes);
}

} // namespace detail
} // namespace ecs

namespace std {
template <> struct hash<ecs::MaterialSlotRequirement> {
  auto operator()(ecs::MaterialSlotRequirement const& k) const noexcept
    -> size_t {
    return std::hash<std::type_index>{}(k.type);
  }
};
} // namespace std
