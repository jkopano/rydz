#pragma once

#include "rydz_ecs/params.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/gl/shader_bindings.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <array>
#include <type_traits>
#include <vector>

namespace gl {

struct Material;

struct MaterialMap {
  ::rlTexture texture{};
  ecs::Color color{};
  f32 value{};

  constexpr MaterialMap() = default;
  constexpr MaterialMap(::rlMaterialMap const& raw) noexcept
      : texture(raw.texture), color(raw.color), value(raw.value) {}

  constexpr operator ::rlMaterialMap() const noexcept {
    return ::rlMaterialMap{texture, color, value};
  }

  auto operator=(::rlMaterialMap const& raw) noexcept -> MaterialMap& {
    *this = MaterialMap(raw);
    return *this;
  }

  [[nodiscard]] auto has_texture() const -> bool { return texture.id > 0; }
};

static_assert(sizeof(MaterialMap) == sizeof(::rlMaterialMap));
static_assert(alignof(MaterialMap) == alignof(::rlMaterialMap));
static_assert(std::is_trivially_copyable_v<MaterialMap>);

struct Mesh {
  i32 vertexCount{};
  i32 triangleCount{};
  f32* vertices{};
  f32* texcoords{};
  f32* texcoords2{};
  f32* normals{};
  f32* tangents{};
  u8* colors{};
  u16* indices{};
  i32 boneCount{};
  u8* boneIndices{};
  f32* boneWeights{};
  f32* animVertices{};
  f32* animNormals{};
  u32 vaoId{};
  u32* vboId{};
  u8 name[64]{};
  i32 id{};
  i32 parentId{};

  constexpr Mesh() = default;
  constexpr Mesh(::rlMesh const& raw) noexcept : Mesh(detail::raylib_cast<Mesh>(raw)) {}

  constexpr operator ::rlMesh() const noexcept {
    return detail::raylib_cast<::rlMesh>(*this);
  }

  auto operator=(::rlMesh const& raw) noexcept -> Mesh& {
    *this = Mesh(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool {
    return vertexCount > 0 || triangleCount > 0 || vaoId != 0;
  }

  [[nodiscard]] auto uploaded() const -> bool { return vaoId != 0; }
  [[nodiscard]] auto vertex_count() const -> int { return vertexCount; }
  [[nodiscard]] auto vertex_data() const -> float const* { return vertices; }
  [[nodiscard]] auto vertex_data() -> float* { return vertices; }

  auto gen_tangents() -> void {
    auto raw = static_cast<::rlMesh>(*this);
    rl::GenMeshTangents(&raw);
    *this = raw;
  }

  auto upload(bool dynamic) -> void {
    auto raw = static_cast<::rlMesh>(*this);
    rl::UploadMesh(&raw, dynamic);
    *this = raw;
  }

  auto unload() -> void {
    if (Mesh::ready()) {
      rl::UnloadMesh(*this);
      *this = Mesh{};
    }
  }
  auto update_buffer(i32 index, void const* data, i32 data_size, i32 offset) const
    -> void {
    rl::UpdateMeshBuffer(*this, index, data, data_size, offset);
  }

  auto draw_instanced(Material const& material, Matrix const* transforms, i32 count) const
    -> void;

  static auto prepare_generated_mesh(Mesh& mesh) -> void { mesh.gen_tangents(); }

  static auto cube(float width = 1, float height = 1, float length = 1) -> Mesh {
    auto mesh = Mesh{rl::GenMeshCube(width, height, length)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto sphere(float radius = 0.5f, int rings = 16, int slices = 16) -> Mesh {
    auto mesh = Mesh{rl::GenMeshSphere(radius, rings, slices)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto plane(float width = 10, float length = 10, int res_x = 1, int res_z = 1)
    -> Mesh {
    auto mesh = Mesh{rl::GenMeshPlane(width, length, res_x, res_z)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto cylinder(float radius = 0.5f, float height = 1, int slices = 16) -> Mesh {
    auto mesh = Mesh{rl::GenMeshCylinder(radius, height, slices)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto torus(
    float radius = 0.5f, float size = 0.2f, int rad_seg = 16, int sides = 16
  ) -> Mesh {
    auto mesh = Mesh{rl::GenMeshTorus(radius, size, rad_seg, sides)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto hemisphere(float radius = 0.5f, int rings = 16, int slices = 16) -> Mesh {
    auto mesh = Mesh{rl::GenMeshHemiSphere(radius, rings, slices)};
    prepare_generated_mesh(mesh);
    return mesh;
  }

  static auto knot(
    float radius = 0.5f, float size = 0.2f, int rad_seg = 16, int sides = 16
  ) -> Mesh {
    auto mesh = Mesh{rl::GenMeshKnot(radius, size, rad_seg, sides)};
    prepare_generated_mesh(mesh);
    return mesh;
  }
};

static_assert(sizeof(Mesh) == sizeof(::rlMesh));
static_assert(alignof(Mesh) == alignof(::rlMesh));
static_assert(std::is_trivially_copyable_v<Mesh>);

struct Material {
  Shader shader{};
  MaterialMap* maps{};
  std::array<f32, 4> params{};

  constexpr Material() = default;
  constexpr Material(::rlMaterial const& raw) noexcept
      : Material(detail::raylib_cast<Material>(raw)) {}

  constexpr operator ::rlMaterial() const noexcept {
    return detail::raylib_cast<::rlMaterial>(*this);
  }

  auto operator=(::rlMaterial const& raw) noexcept -> Material& {
    *this = Material(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return shader.ready() || maps != nullptr; }

  static auto fallback_material(ecs::NonSendMarker) -> gl::Material& {
    static gl::Material fallback = {};
    static bool init = false;
    if (!init) {
      fallback.shader = Shader::get_default();
      static std::vector<gl::MaterialMap> maps(ecs::K_MATERIAL_MAP_COUNT);
      fallback.maps = maps.data();
      constexpr int albedo = ecs::material_map_index(ecs::MaterialMap::Albedo);
      fallback.maps[albedo].texture.id = rl::rlGetTextureIdDefault();
      fallback.maps[albedo].texture.width = 1;
      fallback.maps[albedo].texture.height = 1;
      fallback.maps[albedo].texture.mipmaps = 1;
      fallback.maps[albedo].texture.format = gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
      fallback.maps[albedo].color = ecs::Color::WHITE;
      init = true;
    }
    return fallback;
  }
};

static_assert(sizeof(Material) == sizeof(::rlMaterial));
static_assert(alignof(Material) == alignof(::rlMaterial));
static_assert(std::is_trivially_copyable_v<Material>);

inline auto Mesh::draw_instanced(
  Material const& material, Matrix const* transforms, i32 count
) const -> void {
  rl::DrawMeshInstanced(*this, material, transforms, count);
}

} // namespace gl
