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
#include "rydz_graphics/shader_bindings.hpp"
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
  MaterialMap map_type = MaterialMap::Albedo;
  Handle<Texture> texture{};
  Color color = Color::WHITE;
  f32 value = 0.0f;
  bool has_texture = false;
  bool has_color = false;
  bool has_value = false;

  auto operator==(MaterialMapBinding const& o) const -> bool {
    return map_type == o.map_type && texture == o.texture &&
           color.r == o.color.r && color.g == o.color.g &&
           color.b == o.color.b && color.a == o.color.a && value == o.value &&
           has_texture == o.has_texture && has_color == o.has_color &&
           has_value == o.has_value;
  }

  static auto texture_binding(MaterialMap map_type, Handle<Texture> texture)
    -> MaterialMapBinding {
    return MaterialMapBinding{
      .map_type = map_type,
      .texture = texture,
      .has_texture = texture.is_valid(),
    };
  }

  static auto color_binding(MaterialMap map_type, Color color)
    -> MaterialMapBinding {
    return MaterialMapBinding{
      .map_type = map_type,
      .color = color,
      .has_color = true,
    };
  }

  static auto value_binding(MaterialMap map_type, f32 value)
    -> MaterialMapBinding {
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

struct CompiledMaterial {
  ShaderSpec shader;
  std::vector<MaterialMapBinding> maps;
  std::unordered_map<UniformName, Uniform> uniforms;
  std::vector<MaterialSlotRequirement> slots;
  RenderMethod render_method = RenderMethod::Opaque;
  bool casts_shadows = true;
  bool double_sided = false;
  f32 alpha_cutoff = 0.1f;
  std::string material_type_name;

  auto operator==(CompiledMaterial const& o) const -> bool {
    return shader == o.shader && maps == o.maps && uniforms == o.uniforms &&
           slots == o.slots && render_method == o.render_method &&
           casts_shadows == o.casts_shadows && double_sided == o.double_sided &&
           alpha_cutoff == o.alpha_cutoff;
  }

  auto set_uniform(std::string_view name, Uniform u) -> void {
    uniforms.insert_or_assign(std::string{name}, std::move(u));
  }

  template <ShaderUniformBinding Binding>
  auto set_uniform(Binding binding, Uniform u) -> void {
    set_uniform(map_uniform_binding(binding), std::move(u));
  }

  auto has_uniform_named(std::string_view name) const -> bool {
    return this->uniforms.contains(std::string{name});
  }

  template <ShaderUniformBinding Binding>
  auto has_uniform(Binding binding) const -> bool {
    return has_uniform_named(map_uniform_binding(binding));
  }

  [[nodiscard]] auto cull_state() const -> gl::Cull {
    if (this->double_sided) {
      return gl::Cull::none();
    }

    return gl::Cull::back();
  }

  auto apply(ShaderProgram& shader) const -> void {
    shader.set(StandardMaterialUniform::AlphaCutoff, this->alpha_cutoff);
    shader.set(
      StandardMaterialUniform::RenderMethod,
      static_cast<int>(this->render_method)
    );
    for (auto const& [name, uniform] : this->uniforms) {
      shader.apply(name, uniform);
    }
  }
};

class MaterialBuilder {
  std::vector<MaterialMapBinding> maps_;
  std::unordered_map<UniformName, Uniform> uniforms_;

public:
  auto uniform(std::string_view name, Uniform value) -> MaterialBuilder& {
    uniforms_.insert_or_assign(std::string{name}, value);
    return *this;
  }

  template <ShaderUniformBinding Binding>
  auto uniform(Binding binding, Uniform value) -> MaterialBuilder& {
    return uniform(map_uniform_binding(binding), std::move(value));
  }

  auto texture(MaterialMap map_type, Handle<Texture> texture)
    -> MaterialBuilder& {
    maps_.push_back(MaterialMapBinding::texture_binding(map_type, texture));
    return *this;
  }

  auto color(MaterialMap map_type, Color color_value) -> MaterialBuilder& {
    maps_.push_back(MaterialMapBinding::color_binding(map_type, color_value));
    return *this;
  }

  auto maps() const -> std::vector<MaterialMapBinding> const& { return maps_; }
  auto uniforms() const -> std::unordered_map<UniformName, Uniform> const& {
    return uniforms_;
  }

  auto take_maps() -> std::vector<MaterialMapBinding> {
    return std::move(maps_);
  }
  auto take_uniforms() -> std::unordered_map<UniformName, Uniform> {
    return std::move(uniforms_);
  }
};

template <typename T>
concept IsMaterial = requires(T const m, MaterialBuilder& builder) {
  typename T::_material_have_to_implement;
  { m.bind(builder) } -> std::same_as<void>;
  { m.render_method() } -> std::same_as<RenderMethod>;
  { m.enable_shadows() } -> std::same_as<bool>;
  { m.double_sided() } -> std::same_as<bool>;
  { m.alpha_cutoff() } -> std::same_as<f32>;

  { T::vertex_shader() } -> std::convertible_to<ShaderRef>;
  { T::fragment_shader() } -> std::convertible_to<ShaderRef>;
};

template <typename... SlotTs> struct MaterialTrait {
  static auto vertex_shader() -> ShaderRef { return ""; }
  static auto fragment_shader() -> ShaderRef { return ""; }
  auto bind(MaterialBuilder&) const -> void {}
  [[nodiscard]] auto render_method() const -> RenderMethod {
    return RenderMethod::Opaque;
  }
  [[nodiscard]] auto enable_shadows() const -> bool { return true; }
  [[nodiscard]] auto double_sided() const -> bool { return false; }
  [[nodiscard]] auto alpha_cutoff() const -> f32 { return 0.1f; }
  using _material_have_to_implement = void;

private:
  using _slot_tuple = std::tuple<SlotTs...>;
  template <typename, typename> friend struct MaterialMeta;
};

template <typename M, typename = void> struct MaterialMeta {
  static constexpr bool is_trait_based = false;
};

template <typename M>
struct MaterialMeta<M, std::void_t<typename bare_t<M>::_slot_tuple>> {
  using slot_tuple = typename bare_t<M>::_slot_tuple;
  static constexpr bool is_trait_based = true;
};

template <typename M>
concept TraitMaterialValue = MaterialMeta<bare_t<M>>::is_trait_based;

template <typename M>
concept MaterialValue = TraitMaterialValue<M>;

namespace detail {

inline auto remove_uniform_by_name(
  std::unordered_map<UniformName, Uniform>& uniforms, std::string_view name
) -> std::optional<Uniform> {
  auto it = uniforms.find(std::string{name});
  if (it == uniforms.end()) {
    return std::nullopt;
  }

  Uniform removed = it->second;
  uniforms.erase(it);
  return removed;
}

inline auto compiled_material_has_slot(
  CompiledMaterial const& compiled, std::type_index slot_type
) -> bool {
  return std::ranges::any_of(compiled.slots, [&](auto const& slot) -> auto {
    return slot.type == slot_type;
  });
}

inline auto find_last_map_binding(
  CompiledMaterial const& compiled, MaterialMap map_type, auto predicate
) -> MaterialMapBinding const* {
  for (auto const& map : std::ranges::reverse_view(compiled.maps)) {
    if (map.map_type == map_type && predicate(map)) {
      return &map;
    }
  }
  return nullptr;
}

inline auto normalize_pbr_compiled_material(CompiledMaterial& compiled)
  -> void {
  if (!compiled.has_uniform(MaterialMap::Metalness)) {
    f32 metallic = 0.0f;
    if (auto const* binding = find_last_map_binding(
          compiled, MaterialMap::Metalness, [](auto const& item) -> auto {
            return item.has_value;
          }
        )) {
      metallic = binding->value;
    }
    compiled.set_uniform(MaterialMap::Metalness, Uniform{metallic});
  }

  if (!compiled.has_uniform(MaterialMap::Roughness)) {
    f32 roughness = 0.0f;
    auto const* value_binding = find_last_map_binding(
      compiled, MaterialMap::Roughness, [](auto const& item) -> auto {
        return item.has_value;
      }
    );
    auto const* texture_binding = find_last_map_binding(
      compiled, MaterialMap::Roughness, [](auto const& item) -> auto {
        return item.has_texture;
      }
    );
    if (value_binding != nullptr) {
      roughness = value_binding->value;
    } else if ((texture_binding == nullptr) ||
               !texture_binding->texture.is_valid()) {
      roughness = 1.0f;
    }
    compiled.set_uniform(MaterialMap::Roughness, Uniform{roughness});
  }

  if (!compiled.has_uniform(MaterialMap::Normal)) {
    f32 normal = 1.0f;
    if (auto const* binding = find_last_map_binding(
          compiled, MaterialMap::Normal, [](auto const& item) -> auto {
            return item.has_value;
          }
        )) {
      normal = binding->value > 0.0f ? binding->value : 1.0f;
    }
    compiled.set_uniform(MaterialMap::Normal, Uniform{normal});
  }

  if (!compiled.has_uniform(MaterialMap::Occlusion)) {
    f32 occlusion = 1.0f;
    if (auto const* binding = find_last_map_binding(
          compiled, MaterialMap::Occlusion, [](auto const& item) -> auto {
            return item.has_value;
          }
        )) {
      occlusion = binding->value > 0.0f ? binding->value : 1.0f;
    }
    compiled.set_uniform(MaterialMap::Occlusion, Uniform{occlusion});
  }

  if (!compiled.has_uniform(MaterialMap::Emission)) {
    f32 ex = 0.0f;
    f32 ey = 0.0f;
    f32 ez = 0.0f;
    if (auto const* binding = find_last_map_binding(
          compiled, MaterialMap::Emission, [](auto const& item) -> auto {
            return item.has_color;
          }
        )) {
      math::Vec3 emissive = binding->color;
      ex = emissive.x;
      ey = emissive.y;
      ez = emissive.z;
    }
    compiled.set_uniform(MaterialMap::Emission, Uniform{ex, ey, ez});
  }

  if (!compiled.has_uniform(MaterialMap::Albedo)) {
    compiled.set_uniform(MaterialMap::Albedo, Uniform{1.0f, 1.0f, 1.0f, 0.0f});
  }
}

template <IsMaterial M>
auto compile_trait_material(M const& material) -> CompiledMaterial {
  using T = bare_t<M>;
  static_assert(MaterialMeta<T>::is_trait_based);

  CompiledMaterial compiled;
  compiled.shader = ShaderSpec::from(
    std::string(T::vertex_shader()), std::string(T::fragment_shader())
  );
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
  Material(M const& material)
      : compiled(detail::compile_trait_material(material)) {}
};

template <typename M>
concept RenderMaterialAsset =
  std::same_as<bare_t<M>, Material> || MaterialValue<bare_t<M>>;

template <RenderMaterialAsset M>
auto compile_render_material_asset(M const& material) -> CompiledMaterial {
  if constexpr (std::same_as<bare_t<M>, Material>) {
    return material.compiled;
  } else {
    return detail::compile_trait_material(material);
  }
}

template <typename M = Material> struct MeshMaterial3d {
  Handle<M> material{};

  MeshMaterial3d() = default;
  explicit MeshMaterial3d(Handle<M> material) : material(material) {}

  auto operator*() -> Handle<M>& { return material; }
  auto operator*() const -> Handle<M> const& { return material; }
};

} // namespace ecs

namespace std {
template <> struct hash<ecs::MaterialMapBinding> {
  auto operator()(ecs::MaterialMapBinding const& k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(
      seed, std::hash<i32>{}(ecs::material_map_index(k.map_type))
    );
    rydz::hash_combine(seed, std::hash<u32>{}(k.texture.id));
    rydz::hash_combine(seed, std::hash<u8>{}(k.color.r));
    rydz::hash_combine(seed, std::hash<u8>{}(k.color.g));
    rydz::hash_combine(seed, std::hash<u8>{}(k.color.b));
    rydz::hash_combine(seed, std::hash<u8>{}(k.color.a));
    rydz::hash_combine(seed, std::hash<f32>{}(k.value));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_texture));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_color));
    rydz::hash_combine(seed, std::hash<bool>{}(k.has_value));
    return seed;
  }
};

template <> struct hash<ecs::CompiledMaterial> {
  auto operator()(ecs::CompiledMaterial const& k) const noexcept -> size_t {
    size_t seed = 0;
    rydz::hash_combine(seed, std::hash<ecs::ShaderSpec>{}(k.shader));
    rydz::hash_combine(
      seed, std::hash<i32>{}(static_cast<int>(k.render_method))
    );
    rydz::hash_combine(seed, std::hash<bool>{}(k.casts_shadows));
    rydz::hash_combine(seed, std::hash<bool>{}(k.double_sided));
    rydz::hash_combine(seed, std::hash<f32>{}(k.alpha_cutoff));
    for (auto const& slot : k.slots) {
      rydz::hash_combine(seed, std::hash<ecs::MaterialSlotRequirement>{}(slot));
    }
    for (auto const& map : k.maps) {
      rydz::hash_combine(seed, std::hash<ecs::MaterialMapBinding>{}(map));
    }
    for (auto const& [name, uniform] : k.uniforms) {
      rydz::hash_combine(seed, std::hash<std::string>{}(name));
      rydz::hash_combine(seed, std::hash<ecs::Uniform>{}(uniform));
    }
    return seed;
  }
};
} // namespace std
