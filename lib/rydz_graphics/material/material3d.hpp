#pragma once

#include "hash.hpp"
#include "material_slot.hpp"
#include "math.hpp"
#include "rydz_ecs/asset.hpp"
#include "rydz_ecs/helpers.hpp"
#include "rydz_graphics/color.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include "rydz_graphics/render_config.hpp"
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

using gl::Mesh;
using gl::ShaderProgram;
using gl::ShaderSpec;
using gl::Texture;
using gl::Uniform;
using gl::UniformType;

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

  void apply(ShaderProgram &shader) const {
    shader.set("u_alpha_cutoff", this->alpha_cutoff);
    shader.set("u_render_method", static_cast<int>(this->render_method));
    shader.set("u_use_instancing", 0);
    for (const auto &[name, uniform] : this->uniforms) {
      shader.apply(std::string(name), uniform);
    }
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
