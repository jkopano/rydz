#pragma once

#include "hash.hpp"
#include "math.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/helpers.hpp"
#include "rydz_graphics/assets/types.hpp"
#include "rydz_graphics/color.hpp"
#include "rydz_graphics/render_config.hpp"
#include "rydz_graphics/shader.hpp"
#include <algorithm>
#include <concepts>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ecs {

struct MaterialMapBinding {
  gl::MaterialMapIndex map_type = gl::MATERIAL_MAP_DIFFUSE;
  Handle<Texture> texture{};
  Color color = kWhite;
  f32 value = 0.0f;
  bool has_texture = false;
  bool has_color = false;
  bool has_value = false;

  bool operator==(const MaterialMapBinding &o) const {
    return map_type == o.map_type && texture == o.texture &&
           color.r == o.color.r && color.g == o.color.g &&
           color.b == o.color.b && color.a == o.color.a && value == o.value &&
           has_texture == o.has_texture && has_color == o.has_color &&
           has_value == o.has_value;
  }

  static MaterialMapBinding texture_binding(gl::MaterialMapIndex map_type,
                                            Handle<Texture> texture) {
    return MaterialMapBinding{
        .map_type = map_type,
        .texture = texture,
        .has_texture = texture.is_valid(),
    };
  }

  static MaterialMapBinding color_binding(gl::MaterialMapIndex map_type,
                                          Color color) {
    return MaterialMapBinding{
        .map_type = map_type,
        .color = color,
        .has_color = true,
    };
  }

  static MaterialMapBinding value_binding(gl::MaterialMapIndex map_type,
                                          f32 value) {
    return MaterialMapBinding{
        .map_type = map_type,
        .value = value,
        .has_value = true,
    };
  }
};

enum class RenderMethod {
  Opaque,
  Transparent,
  AlphaCutout,
};

using ShaderRef = std::string_view;
using UniformName = std::string_view;

struct HasCamera {};

struct HasPBR {
  using DependsOn = std::tuple<HasCamera>;
};

struct MaterialSlotRequirement {
  std::type_index type = typeid(void);
  std::string debug_name;

  bool operator==(const MaterialSlotRequirement &o) const {
    return type == o.type;
  }
};

struct CompiledMaterial {
  ShaderSpec shader;
  std::vector<MaterialMapBinding> maps;
  std::unordered_map<UniformName, Uniform> uniforms;
  std::vector<MaterialSlotRequirement> slots;
  RenderMethod render_method = RenderMethod::Opaque;
  bool casts_shadows = true;
  bool double_sided = false;
  float alpha_cutoff = 0.1f;
  std::string material_type_name;

  bool operator==(const CompiledMaterial &o) const {
    return shader == o.shader && maps == o.maps && uniforms == o.uniforms &&
           slots == o.slots && render_method == o.render_method &&
           casts_shadows == o.casts_shadows && double_sided == o.double_sided &&
           alpha_cutoff == o.alpha_cutoff;
  }

  void set_uniform(std::string_view name, Uniform u) {
    uniforms.insert_or_assign(name, std::move(u));
  }

  bool has_uniform_named(std::string_view name) {
    return this->uniforms.contains(name);
  }

  void apply_cull_mode() const {
    if (this->double_sided) {
      gl::disable_backface_culling();
      return;
    }

    gl::enable_backface_culling();
    gl::set_cull_face(gl::CullFace::Back);
  }
};

class MaterialBuilder {
  std::vector<MaterialMapBinding> maps_;
  std::unordered_map<std::string_view, Uniform> uniforms_;

public:
  MaterialBuilder &uniform(std::string_view name, Uniform value) {
    uniforms_.insert_or_assign(name, value);
    return *this;
  }

  MaterialBuilder &texture(gl::MaterialMapIndex map_type,
                           Handle<Texture> texture) {
    maps_.push_back(MaterialMapBinding::texture_binding(map_type, texture));
    return *this;
  }

  MaterialBuilder &color(gl::MaterialMapIndex map_type, Color color_value) {
    maps_.push_back(MaterialMapBinding::color_binding(map_type, color_value));
    return *this;
  }

  const std::vector<MaterialMapBinding> &maps() const { return maps_; }
  const std::unordered_map<std::string_view, Uniform> &uniforms() const {
    return uniforms_;
  }

  std::vector<MaterialMapBinding> take_maps() { return std::move(maps_); }
  std::unordered_map<std::string_view, Uniform> take_uniforms() {
    return std::move(uniforms_);
  }
};
;

template <typename... SlotTs> struct MaterialTrait {
  static ShaderRef vertex_shader() { return "res/shaders/basic.vert"; }
  static ShaderRef fragment_shader() { return "res/shaders/basic.frag"; }

  RenderMethod render_method() const { return RenderMethod::Opaque; }
  bool enable_shadows() const { return true; }
  bool double_sided() const { return false; }
  float alpha_cutoff() const { return 0.1f; }
  void bind(MaterialBuilder &) const {}

private:
  using __slot_tuple = std::tuple<SlotTs...>;
  template <typename, typename> friend struct MaterialMeta;
};

template <typename M, typename = void> struct MaterialMeta {
  static constexpr bool is_trait_based = false;
};

template <typename M>
struct MaterialMeta<M, std::void_t<typename bare_t<M>::__slot_tuple>> {
  using slot_tuple = typename bare_t<M>::__slot_tuple;
  static constexpr bool is_trait_based = true;
};

template <typename M>
concept TraitMaterialValue = MaterialMeta<bare_t<M>>::is_trait_based;

template <typename M>
concept MaterialValue = TraitMaterialValue<M>;

namespace detail {

inline bool is_reserved_uniform_name(std::string_view name) {
  static const std::unordered_set<std::string_view> reserved = {
      "u_alpha_cutoff",        "u_use_instancing",
      "u_camera_pos",          "matView",
      "u_dir_light_direction", "u_dir_light_intensity",
      "u_dir_light_color",     "u_has_directional",
      "u_cluster_dimensions",  "u_cluster_screen_size",
      "u_cluster_near_far",    "u_cluster_max_lights",
      "u_is_orthographic",     "u_render_method",
  };
  return reserved.contains(name);
}

inline void validate_authored_uniforms(
    const std::unordered_map<UniformName, Uniform> &uniforms,
    const std::string &material_type_name) {
  for (const auto &[name, uniform] : uniforms) {
    if (is_reserved_uniform_name(name)) {
      throw std::runtime_error("Material '" + material_type_name +
                               "' writes reserved uniform '" +
                               std::string(name) + "'");
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

inline SlotGraphNode &find_or_add_slot_node(std::vector<SlotGraphNode> &nodes,
                                            std::type_index type,
                                            std::string debug_name) {
  auto it = std::find_if(nodes.begin(), nodes.end(),
                         [&](const auto &node) { return node.type == type; });
  if (it != nodes.end()) {
    return *it;
  }

  nodes.push_back(SlotGraphNode{
      .type = type,
      .debug_name = std::move(debug_name),
      .deps = {},
  });
  return nodes.back();
}

inline const SlotGraphNode &
find_slot_node(const std::vector<SlotGraphNode> &nodes, std::type_index type) {
  auto it = std::find_if(nodes.begin(), nodes.end(),
                         [&](const auto &node) { return node.type == type; });
  if (it == nodes.end()) {
    throw std::logic_error("Material slot graph is missing a dependency node");
  }
  return *it;
}

template <typename Tuple, std::size_t... I>
void collect_tuple_slots(std::vector<SlotGraphNode> &nodes,
                         std::vector<std::type_index> &visiting,
                         std::index_sequence<I...>);

template <typename Slot>
void collect_slot_graph(std::vector<SlotGraphNode> &nodes,
                        std::vector<std::type_index> &visiting) {
  const std::type_index slot_type = typeid(Slot);
  if (std::find(visiting.begin(), visiting.end(), slot_type) !=
      visiting.end()) {
    throw std::runtime_error("Slot dependency cycle detected at '" +
                             demangle(typeid(Slot).name()) + "'");
  }

  {
    auto &node =
        find_or_add_slot_node(nodes, slot_type, demangle(typeid(Slot).name()));
    if (!node.deps.empty()) {
      return;
    }
  }

  visiting.push_back(slot_type);
  using DependsTuple = typename SlotDependsOnTuple<Slot>::type;
  if constexpr (std::tuple_size_v<DependsTuple> > 0) {
    collect_tuple_slots<DependsTuple>(
        nodes, visiting,
        std::make_index_sequence<std::tuple_size_v<DependsTuple>>{});

    std::vector<std::type_index> deps;
    deps.reserve(std::tuple_size_v<DependsTuple>);
    [&]<std::size_t... I>(std::index_sequence<I...>) {
      (deps.push_back(typeid(std::tuple_element_t<I, DependsTuple>)), ...);
    }(std::make_index_sequence<std::tuple_size_v<DependsTuple>>{});

    std::sort(deps.begin(), deps.end(), [&](const auto &lhs, const auto &rhs) {
      const auto &lhs_name = find_slot_node(nodes, lhs).debug_name;
      const auto &rhs_name = find_slot_node(nodes, rhs).debug_name;
      return lhs_name < rhs_name;
    });
    deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
    auto &node =
        find_or_add_slot_node(nodes, slot_type, demangle(typeid(Slot).name()));
    node.deps = std::move(deps);
  }
  visiting.pop_back();
}

template <typename Tuple, std::size_t... I>
void collect_tuple_slots(std::vector<SlotGraphNode> &nodes,
                         std::vector<std::type_index> &visiting,
                         std::index_sequence<I...>) {
  (collect_slot_graph<std::tuple_element_t<I, Tuple>>(nodes, visiting), ...);
}

inline std::vector<MaterialSlotRequirement>
topologically_sorted_slots(const std::vector<SlotGraphNode> &nodes) {
  std::unordered_map<std::type_index, usize> indegree;
  std::unordered_map<std::type_index, std::vector<std::type_index>> adjacency;
  std::unordered_map<std::type_index, const SlotGraphNode *> node_ptr;

  for (const auto &node : nodes) {
    node_ptr[node.type] = &node;
    indegree[node.type] = node.deps.size();
    for (const auto &dep : node.deps) {
      adjacency[dep].push_back(node.type);
    }
  }

  auto cmp = [&](std::type_index a, std::type_index b) {
    return node_ptr[a]->debug_name < node_ptr[b]->debug_name;
  };
  std::set<std::type_index, decltype(cmp)> ready(cmp);

  for (const auto &node : nodes) {
    if (node.deps.empty())
      ready.insert(node.type);
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

template <typename Tuple> std::vector<MaterialSlotRequirement> expand_slots() {
  std::vector<SlotGraphNode> nodes;
  nodes.reserve(std::tuple_size_v<Tuple>);
  std::vector<std::type_index> visiting;
  collect_tuple_slots<Tuple>(
      nodes, visiting, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
  return topologically_sorted_slots(nodes);
}

inline std::optional<Uniform>
remove_uniform_by_name(std::unordered_map<UniformName, Uniform> &uniforms,
                       std::string_view name) {
  auto it = uniforms.find(name);
  if (it == uniforms.end()) {
    return std::nullopt;
  }

  Uniform removed = std::move(it->second);
  uniforms.erase(it);
  return removed;
}

inline bool compiled_material_has_slot(const CompiledMaterial &compiled,
                                       std::type_index slot_type) {
  return std::ranges::any_of(
      compiled.slots, [&](const auto &slot) { return slot.type == slot_type; });
}

inline const MaterialMapBinding *
find_last_map_binding(const CompiledMaterial &compiled,
                      gl::MaterialMapIndex map_type, auto predicate) {
  for (auto it = compiled.maps.rbegin(); it != compiled.maps.rend(); ++it) {
    if (it->map_type == map_type && predicate(*it)) {
      return &*it;
    }
  }
  return nullptr;
}

inline void normalize_pbr_compiled_material(CompiledMaterial &compiled) {
  if (!compiled.has_uniform_named("u_metallic_factor")) {
    float metallic = 0.0f;
    if (const auto *binding = find_last_map_binding(
            compiled, gl::MATERIAL_MAP_METALNESS,
            [](const auto &item) { return item.has_value; })) {
      metallic = binding->value;
    }
    compiled.set_uniform("u_metallic_factor", Uniform{metallic});
  }

  if (!compiled.has_uniform_named("u_roughness_factor")) {
    float roughness = 0.0f;
    const auto *value_binding =
        find_last_map_binding(compiled, gl::MATERIAL_MAP_ROUGHNESS,
                              [](const auto &item) { return item.has_value; });
    const auto *texture_binding = find_last_map_binding(
        compiled, gl::MATERIAL_MAP_ROUGHNESS,
        [](const auto &item) { return item.has_texture; });
    if (value_binding) {
      roughness = value_binding->value;
    } else if (!texture_binding || !texture_binding->texture.is_valid()) {
      roughness = 1.0f;
    }
    compiled.set_uniform("u_roughness_factor", Uniform{roughness});
  }

  if (!compiled.has_uniform_named("u_normal_factor")) {
    float normal = 1.0f;
    if (const auto *binding = find_last_map_binding(
            compiled, gl::MATERIAL_MAP_NORMAL,
            [](const auto &item) { return item.has_value; })) {
      normal = binding->value > 0.0f ? binding->value : 1.0f;
    }
    compiled.set_uniform("u_normal_factor", Uniform{normal});
  }

  if (!compiled.has_uniform_named("u_occlusion_factor")) {
    float occlusion = 1.0f;
    if (const auto *binding = find_last_map_binding(
            compiled, gl::MATERIAL_MAP_OCCLUSION,
            [](const auto &item) { return item.has_value; })) {
      occlusion = binding->value > 0.0f ? binding->value : 1.0f;
    }
    compiled.set_uniform("u_occlusion_factor", Uniform{occlusion});
  }

  if (!compiled.has_uniform_named("u_emissive_factor")) {
    float ex = 0.0f;
    float ey = 0.0f;
    float ez = 0.0f;
    if (const auto *binding = find_last_map_binding(
            compiled, gl::MATERIAL_MAP_EMISSION,
            [](const auto &item) { return item.has_color; })) {
      auto emissive = color_to_vec3(binding->color);
      ex = emissive.x;
      ey = emissive.y;
      ez = emissive.z;
    }
    compiled.set_uniform("u_emissive_factor", Uniform{ex, ey, ez});
  }

  if (!compiled.has_uniform_named("u_color")) {
    compiled.set_uniform("u_color", Uniform{1.0f, 1.0f, 1.0f, 0.0f});
  }
}

template <typename M>
CompiledMaterial compile_trait_material(const M &material) {
  using T = bare_t<M>;
  static_assert(MaterialMeta<T>::is_trait_based);

  CompiledMaterial compiled;
  compiled.shader = ShaderSpec::from(std::string(T::vertex_shader()),
                                     std::string(T::fragment_shader()));
  MaterialBuilder builder;
  material.bind(builder);
  compiled.maps = builder.take_maps();
  compiled.uniforms = builder.take_uniforms();
  compiled.render_method = material.render_method();
  compiled.casts_shadows = material.enable_shadows();
  compiled.double_sided = material.double_sided();
  compiled.alpha_cutoff = material.alpha_cutoff();
  compiled.material_type_name = demangle(typeid(T).name());

  validate_authored_uniforms(compiled.uniforms, compiled.material_type_name);

  compiled.slots = expand_slots<typename MaterialMeta<T>::slot_tuple>();
  if (compiled_material_has_slot(compiled, typeid(HasPBR))) {
    normalize_pbr_compiled_material(compiled);
  }

  return compiled;
}

} // namespace detail

struct Material {
  CompiledMaterial compiled{};

  Material() = default;
  explicit Material(CompiledMaterial compiled)
      : compiled(std::move(compiled)) {}

  template <TraitMaterialValue M>
    requires(!std::same_as<std::remove_cvref_t<M>, Material>)
  Material(const M &material)
      : compiled(detail::compile_trait_material(material)) {}
};

struct StandardMaterial : MaterialTrait<HasPBR> {
  Color base_color = kWhite;
  Handle<Texture> texture{};
  Handle<Texture> normal_map{};
  Handle<Texture> metallic_map{};
  Handle<Texture> roughness_map{};
  Handle<Texture> occlusion_map{};
  Handle<Texture> emissive_map{};
  Color emissive_color = {0, 0, 0, 0};
  f32 metallic = -1.0f;
  f32 roughness = -1.0f;
  f32 normal_scale = -1.0f;
  f32 occlusion_strength = -1.0f;

  static StandardMaterial from_color(Color c) { return {.base_color = c}; }
  static StandardMaterial from_texture(Handle<Texture> tex,
                                       Color tint = kWhite) {
    return {.base_color = tint, .texture = tex};
  }

  static ShaderRef vertex_shader() { return "res/shaders/pbr.vert"; }
  static ShaderRef fragment_shader() { return "res/shaders/pbr.frag"; }

  RenderMethod render_method() const {
    return base_color.a < 255 ? RenderMethod::Transparent
                              : RenderMethod::Opaque;
  }

  bool enable_shadows() const { return base_color.a == 255; }
  bool double_sided() const { return false; }
  float alpha_cutoff() const { return 0.1f; }

  void bind(MaterialBuilder &builder) const {
    builder.color(gl::MATERIAL_MAP_DIFFUSE, base_color);
    if (texture.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_DIFFUSE, texture);
    }
    if (normal_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_NORMAL, normal_map);
    }
    if (metallic_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_METALNESS, metallic_map);
    }
    if (roughness_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_ROUGHNESS, roughness_map);
    }
    if (occlusion_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_OCCLUSION, occlusion_map);
    }
    if (emissive_map.is_valid()) {
      builder.texture(gl::MATERIAL_MAP_EMISSION, emissive_map);
    }

    if (metallic >= 0.0f) {
      builder.uniform("u_metallic_factor", Uniform(metallic));
    }
    if (roughness >= 0.0f) {
      builder.uniform("u_roughness_factor", Uniform{roughness});
    }
    if (normal_scale >= 0.0f) {
      builder.uniform("u_normal_factor", Uniform{normal_scale});
    }
    if (occlusion_strength >= 0.0f) {
      builder.uniform("u_occlusion_factor", Uniform{occlusion_strength});
    }
    if (emissive_color.a > 0) {
      auto emissive = color_to_vec3(emissive_color);
      builder.uniform("u_emissive_factor",
                      math::Vec3(emissive.x, emissive.y, emissive.z));
    }
    builder.uniform("u_color", Uniform{1.0f, 1.0f, 1.0f, 0.0f});
  }
};

static_assert(MaterialValue<StandardMaterial>);

struct MeshMaterial3d {
  Handle<Material> material{};

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(Handle<Material> material) : material(material) {}
};

} // namespace ecs

namespace std {
template <> struct hash<ecs::MaterialMapBinding> {
  size_t operator()(const ecs::MaterialMapBinding &k) const noexcept {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<int>{}(k.map_type));
    rydz::hash_combine(seed, std::hash<uint32_t>{}(k.texture.id));
    rydz::hash_combine(seed, std::hash<unsigned char>{}(k.color.r));
    rydz::hash_combine(seed, std::hash<unsigned char>{}(k.color.g));
    rydz::hash_combine(seed, std::hash<unsigned char>{}(k.color.b));
    rydz::hash_combine(seed, std::hash<unsigned char>{}(k.color.a));
    rydz::hash_combine(seed, std::hash<float>{}(k.value));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_texture));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_color));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_value));
    return seed;
  }
};

template <> struct hash<ecs::MaterialSlotRequirement> {
  size_t operator()(const ecs::MaterialSlotRequirement &k) const noexcept {
    return std::hash<std::type_index>{}(k.type);
  }
};

template <> struct hash<ecs::CompiledMaterial> {
  size_t operator()(const ecs::CompiledMaterial &k) const noexcept {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<ecs::ShaderSpec>{}(k.shader));
    rydz::hash_combine(seed,
                       std::hash<int>{}(static_cast<int>(k.render_method)));
    rydz::hash_combine(seed, std::hash<bool>{}(k.casts_shadows));
    rydz::hash_combine(seed, std::hash<bool>{}(k.double_sided));
    rydz::hash_combine(seed, std::hash<float>{}(k.alpha_cutoff));
    for (const auto &slot : k.slots) {
      rydz::hash_combine(seed, std::hash<ecs::MaterialSlotRequirement>{}(slot));
    }
    for (const auto &map : k.maps) {
      rydz::hash_combine(seed, std::hash<ecs::MaterialMapBinding>{}(map));
    }
    for (const auto &[name, uniform] : k.uniforms) {
      rydz::hash_combine(seed, std::hash<std::string_view>{}(name));
      rydz::hash_combine(seed, std::hash<ecs::Uniform>{}(uniform));
    }
    return seed;
  }
};
} // namespace std
