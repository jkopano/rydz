#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/params.hpp"
#include "rydz_graphics/shader_bindings.hpp"
#include "types.hpp"
#include <algorithm>
#include <bit>
#include <external/glad.h>
#include <type_traits>
#include <utility>

#include "rydz_log/mod.hpp"

namespace gl {

using Color = rl::Color;
using Vec2 = rl::Vector2;
using Vec3 = rl::Vector3;
using Vec4 = rl::Vector4;
using Matrix = rl::Matrix;
using BoneInfo = ::BoneInfo;
using ModelAnimPose = ::ModelAnimPose;
using ModelSkeleton = ::ModelSkeleton;
using AudioStream = ::AudioStream;

struct Rectangle {
  f32 x = 0.0F;
  f32 y = 0.0F;
  f32 width = 0.0F;
  f32 height = 0.0F;

  constexpr Rectangle() = default;
  constexpr Rectangle(f32 x, f32 y, f32 width, f32 height) noexcept
      : x(x), y(y), width(width), height(height) {}
  constexpr Rectangle(const ::rlRectangle &raw) noexcept
      : x(raw.x), y(raw.y), width(raw.width), height(raw.height) {}

  constexpr operator ::rlRectangle() const noexcept {
    return {.x = x, .y = y, .width = width, .height = height};
  }

  [[nodiscard]] constexpr auto left() const noexcept -> f32 { return x; }
  [[nodiscard]] constexpr auto top() const noexcept -> f32 { return y; }
  [[nodiscard]] constexpr auto right() const noexcept -> f32 {
    return x + width;
  }
  [[nodiscard]] constexpr auto bottom() const noexcept -> f32 {
    return y + height;
  }
  [[nodiscard]] constexpr auto position() const noexcept -> Vec2 {
    return {x, y};
  }
  [[nodiscard]] constexpr auto size() const noexcept -> Vec2 {
    return {width, height};
  }
  [[nodiscard]] constexpr auto center() const noexcept -> Vec2 {
    return {x + width * 0.5F, y + height * 0.5F};
  }
  [[nodiscard]] constexpr auto area() const noexcept -> f32 {
    return width * height;
  }
  [[nodiscard]] constexpr auto empty() const noexcept -> bool {
    return width == 0.0F || height == 0.0F;
  }

  [[nodiscard]] constexpr auto normalized() const noexcept -> Rectangle {
    Rectangle result = *this;
    if (result.width < 0.0F) {
      result.x += result.width;
      result.width = -result.width;
    }
    if (result.height < 0.0F) {
      result.y += result.height;
      result.height = -result.height;
    }
    return result;
  }

  [[nodiscard]] constexpr auto contains(Vec2 point) const noexcept -> bool {
    const Rectangle rect = normalized();
    return point.x >= rect.left() && point.x <= rect.right() &&
           point.y >= rect.top() && point.y <= rect.bottom();
  }

  [[nodiscard]] constexpr auto overlaps(Rectangle other) const noexcept
      -> bool {
    const Rectangle a = normalized();
    const Rectangle b = other.normalized();
    return a.left() < b.right() && a.right() > b.left() &&
           a.top() < b.bottom() && a.bottom() > b.top();
  }

  [[nodiscard]] constexpr auto intersection(Rectangle other) const noexcept
      -> Rectangle {
    const Rectangle a = normalized();
    const Rectangle b = other.normalized();
    const f32 ix = std::max(a.left(), b.left());
    const f32 iy = std::max(a.top(), b.top());
    const f32 ir = std::min(a.right(), b.right());
    const f32 ib = std::min(a.bottom(), b.bottom());
    if (ir <= ix || ib <= iy) {
      return {ix, iy, 0.0F, 0.0F};
    }
    return {ix, iy, ir - ix, ib - iy};
  }

  [[nodiscard]] constexpr auto translated(Vec2 delta) const noexcept
      -> Rectangle {
    return {x + delta.x, y + delta.y, width, height};
  }

  [[nodiscard]] constexpr auto resized(Vec2 new_size) const noexcept
      -> Rectangle {
    return {x, y, new_size.x, new_size.y};
  }

  [[nodiscard]] constexpr auto flipped_y() const noexcept -> Rectangle {
    return {x, y, width, -height};
  }
};

inline constexpr int SHADER_UNIFORM_FLOAT = RL_SHADER_UNIFORM_FLOAT;
inline constexpr int SHADER_UNIFORM_VEC2 = RL_SHADER_UNIFORM_VEC2;
inline constexpr int SHADER_UNIFORM_VEC3 = RL_SHADER_UNIFORM_VEC3;
inline constexpr int SHADER_UNIFORM_VEC4 = RL_SHADER_UNIFORM_VEC4;
inline constexpr int SHADER_UNIFORM_INT = RL_SHADER_UNIFORM_INT;
inline constexpr int SHADER_UNIFORM_IVEC2 = RL_SHADER_UNIFORM_IVEC2;
inline constexpr int SHADER_UNIFORM_IVEC3 = RL_SHADER_UNIFORM_IVEC3;
inline constexpr int SHADER_UNIFORM_IVEC4 = RL_SHADER_UNIFORM_IVEC4;

inline constexpr int PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 =
    ::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
inline constexpr int TEXTURE_FILTER_BILINEAR = ::TEXTURE_FILTER_BILINEAR;
inline constexpr int LOG_WARNING = ::LOG_WARNING;

inline auto default_shader_id() -> unsigned int {
  return rl::rlGetShaderIdDefault();
}
inline auto default_shader_locs() -> int * {
  return rl::rlGetShaderLocsDefault();
}
inline auto default_texture_id() -> unsigned int {
  return rl::rlGetTextureIdDefault();
}

inline constexpr Color kWhite = {255, 255, 255, 255};
inline constexpr Color kBlack = {0, 0, 0, 255};

namespace detail {

template <typename To, typename From>
[[nodiscard]] constexpr auto raylib_cast(const From &value) noexcept -> To {
  static_assert(sizeof(To) == sizeof(From));
  static_assert(alignof(To) == alignof(From));
  static_assert(std::is_trivially_copyable_v<To>);
  static_assert(std::is_trivially_copyable_v<From>);
  return std::bit_cast<To>(value);
}

} // namespace detail

struct Texture;
struct Material;

class Buffer {
public:
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;
  Buffer(Buffer &&) noexcept = default;
  auto operator=(Buffer &&) noexcept -> Buffer & = default;

  virtual ~Buffer() = default;

  [[nodiscard]] virtual auto ready() const -> bool = 0;
  [[nodiscard]] virtual auto id() const -> u32 = 0;

  virtual auto reset() -> void = 0;
  virtual auto update(const void *data, unsigned int size,
                      unsigned int offset = 0) const -> void = 0;
  virtual auto bind(unsigned int index) const -> void = 0;
};

class SSBO final : public Buffer {
public:
  SSBO() = default;
  SSBO(unsigned int size, const void *data, int usage)
      : id_(rl::rlLoadShaderBuffer(size, data, usage)) {}

  SSBO(const SSBO &) = delete;
  auto operator=(const SSBO &) -> SSBO & = delete;
  SSBO(SSBO &&other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(SSBO &&other) noexcept -> SSBO & {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~SSBO() override { reset(); }

  [[nodiscard]] auto ready() const -> bool override { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 override { return id_; }

  auto reset() -> void override {
    if (id_ != 0) {
      rl::rlUnloadShaderBuffer(id_);
      id_ = 0;
    }
  }
  auto update(const void *data, unsigned int size, unsigned int offset) const
      -> void override {
    if (id_ != 0) {
      rl::rlUpdateShaderBuffer(id_, data, size, offset);
    }
  }

  auto bind(unsigned int index) const -> void override {
    if (id_ != 0) {
      rl::rlBindShaderBuffer(id_, index);
    }
  }

private:
  u32 id_ = 0;
};

class UBO final : public Buffer {
public:
  UBO() = default;
  UBO(unsigned int size, const void *data, int usage) {
    glGenBuffers(1, &id_);
    glBindBuffer(GL_UNIFORM_BUFFER, id_);
    glBufferData(GL_UNIFORM_BUFFER, size, data, usage);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }

  UBO(const UBO &) = delete;
  UBO(UBO &&other) noexcept : id_(std::exchange(other.id_, 0)) {}
  auto operator=(const UBO &) -> UBO & = delete;
  auto operator=(UBO &&other) noexcept -> UBO & {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~UBO() override { reset(); }

  [[nodiscard]] auto ready() const -> bool override { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 override { return id_; }

  auto reset() -> void override {
    if (id_ != 0) {
      glDeleteBuffers(1, &id_);
      id_ = 0;
    }
  }

  auto update(const void *data, unsigned int size, unsigned int offset) const
      -> void override {
    if (id_ != 0) {
      glBindBuffer(GL_UNIFORM_BUFFER, id_);
      glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
  }

  auto bind(unsigned int index) const -> void override {
    if (id_ != 0) {
      glBindBufferBase(GL_UNIFORM_BUFFER, index, id_);
    }
  }

private:
  u32 id_ = 0;
};

class VertexArray final {
public:
  VertexArray() = default;

  static auto create() -> VertexArray {
    VertexArray array;
    array.id_ = rl::rlLoadVertexArray();
    return array;
  }

  VertexArray(const VertexArray &) = delete;
  auto operator=(const VertexArray &) -> VertexArray & = delete;
  VertexArray(VertexArray &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  auto operator=(VertexArray &&other) noexcept -> VertexArray & {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexArray() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexArray(id_);
      id_ = 0;
    }
  }

  [[nodiscard]] auto bind() const -> bool {
    if (id_ == 0) {
      return false;
    }
    return rl::rlEnableVertexArray(id_);
  }

  static auto unbind() -> void { rl::rlDisableVertexArray(); }

  auto draw(i32 offset, i32 count) const -> void {
    if (id_ != 0) {
      rl::rlDrawVertexArray(offset, count);
    }
  }

private:
  u32 id_ = 0;
};

class VertexBuffer final {
public:
  VertexBuffer() = default;

  VertexBuffer(const void *data, i32 size, bool dynamic = false)
      : id_(rl::rlLoadVertexBuffer(data, size, dynamic)) {}

  static auto create(const void *data, i32 size, bool dynamic = false)
      -> VertexBuffer {
    return {data, size, dynamic};
  }

  VertexBuffer(const VertexBuffer &) = delete;
  auto operator=(const VertexBuffer &) -> VertexBuffer & = delete;
  VertexBuffer(VertexBuffer &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  auto operator=(VertexBuffer &&other) noexcept -> VertexBuffer & {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexBuffer() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  auto bind() const -> void {
    if (id_ != 0) {
      rl::rlEnableVertexBuffer(id_);
    }
  }

  static auto unbind() -> void { rl::rlDisableVertexBuffer(); }

private:
  u32 id_ = 0;
};

class ElementBuffer final {
public:
  ElementBuffer() = default;

  ElementBuffer(const void *data, i32 size, bool dynamic = false)
      : id_(rl::rlLoadVertexBufferElement(data, size, dynamic)) {}

  static auto create(const void *data, i32 size, bool dynamic = false)
      -> ElementBuffer {
    return {data, size, dynamic};
  }

  ElementBuffer(const ElementBuffer &) = delete;
  auto operator=(const ElementBuffer &) -> ElementBuffer & = delete;
  ElementBuffer(ElementBuffer &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  auto operator=(ElementBuffer &&other) noexcept -> ElementBuffer & {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~ElementBuffer() { reset(); }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto reset() -> void {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  auto bind() const -> void {
    if (id_ != 0) {
      rl::rlEnableVertexBufferElement(id_);
    }
  }

  static auto unbind() -> void { rl::rlDisableVertexBufferElement(); }

  static auto draw(i32 offset, i32 count, const void *buffer = nullptr)
      -> void {
    rl::rlDrawVertexArrayElements(offset, count, buffer);
  }

private:
  u32 id_ = 0;
};

using VAO = VertexArray;
using VBO = VertexBuffer;
using EBO = ElementBuffer;

struct Image {
  void *data = nullptr;
  int width = 0;
  int height = 0;
  int mipmaps = 0;
  int format = 0;

  constexpr Image() = default;
  constexpr Image(const ::Image &raw) noexcept
      : Image(detail::raylib_cast<Image>(raw)) {}

  constexpr operator ::Image() const noexcept {
    return detail::raylib_cast<::Image>(*this);
  }

  auto operator=(const ::Image &raw) noexcept -> Image & {
    *this = Image(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return data != nullptr; }

  auto unload() -> void {
    if (ready()) {
      rl::UnloadImage(*this);
      *this = Image{};
    }
  }

  auto format_to(int pixel_format) -> void {
    auto raw = static_cast<::Image>(*this);
    rl::ImageFormat(&raw, pixel_format);
    *this = raw;
  }

  [[nodiscard]] auto load_texture() const -> Texture;
};

struct Texture {
  u32 id = 0;
  i32 width = 0;
  i32 height = 0;
  i32 mipmaps = 0;
  i32 format = 0;

  constexpr Texture() = default;
  constexpr Texture(const ::Texture &raw) noexcept
      : Texture(detail::raylib_cast<Texture>(raw)) {}

  constexpr operator ::Texture() const noexcept {
    return detail::raylib_cast<::Texture>(*this);
  }

  auto operator=(const ::Texture &raw) noexcept -> Texture & {
    *this = Texture(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id > 0; }

  auto unload() -> void {
    if (ready()) {
      rl::UnloadTexture(*this);
      *this = Texture{};
    }
  }

  auto set_filter(i32 filter) const -> void {
    rl::SetTextureFilter(*this, filter);
  }

  [[nodiscard]] auto rect() const -> Rectangle {
    return {0.0F, 0.0F, static_cast<f32>(width), static_cast<float>(height)};
  }

  [[nodiscard]] auto flipped_rect() const -> Rectangle {
    return {0.0F, 0.0F, static_cast<f32>(width), -static_cast<float>(height)};
  }
};

struct Sound {
  AudioStream stream{};
  unsigned int frameCount = 0;

  constexpr Sound() = default;
  constexpr Sound(const ::Sound &raw) noexcept
      : Sound(detail::raylib_cast<Sound>(raw)) {}

  constexpr operator ::Sound() const noexcept {
    return detail::raylib_cast<::Sound>(*this);
  }

  auto operator=(const ::Sound &raw) noexcept -> Sound & {
    *this = Sound(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool {
    return stream.buffer != nullptr && frameCount > 0;
  }

  auto unload() -> void {
    if (ready()) {
      ::UnloadSound(*this);
      *this = Sound{};
    }
  }
};

struct Shader {
  u32 id = 0;
  i32 *locs = nullptr;

  constexpr Shader() = default;
  constexpr Shader(const ::Shader &raw) noexcept
      : Shader(detail::raylib_cast<Shader>(raw)) {}

  constexpr operator ::Shader() const noexcept {
    return detail::raylib_cast<::Shader>(*this);
  }

  auto operator=(const ::Shader &raw) noexcept -> Shader & {
    *this = Shader(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id != 0; }
  [[nodiscard]] auto has_locations() const -> bool { return locs != nullptr; }

  auto uniform_location(const char *name) const -> i32 {
    return rl::GetShaderLocation(*this, name);
  }

  auto attribute_location(const char *name) const -> i32 {
    return rl::GetShaderLocationAttrib(*this, name);
  }

  static auto get_default() -> Shader {
    Shader shader = {};
    shader.id = default_id();
    shader.locs = default_locs();
    return shader;
  }
  static auto default_id() -> u32 { return rl::rlGetShaderIdDefault(); }
  static auto default_locs() -> i32 * { return rl::rlGetShaderLocsDefault(); }
};

struct MaterialMap {
  Texture texture{};
  Color color{};
  f32 value = 0.0f;

  constexpr MaterialMap() = default;
  constexpr MaterialMap(const ::MaterialMap &raw) noexcept
      : MaterialMap(detail::raylib_cast<MaterialMap>(raw)) {}

  constexpr operator ::MaterialMap() const noexcept {
    return detail::raylib_cast<::MaterialMap>(*this);
  }

  auto operator=(const ::MaterialMap &raw) noexcept -> MaterialMap & {
    *this = MaterialMap(raw);
    return *this;
  }

  [[nodiscard]] auto has_texture() const -> bool { return texture.ready(); }
};

struct Mesh {
  i32 vertexCount = 0;
  i32 triangleCount = 0;
  f32 *vertices = nullptr;
  f32 *texcoords = nullptr;
  f32 *texcoords2 = nullptr;
  f32 *normals = nullptr;
  f32 *tangents = nullptr;
  u8 *colors = nullptr;
  u16 *indices = nullptr;
  i32 boneCount = 0;
  u8 *boneIndices = nullptr;
  f32 *boneWeights = nullptr;
  f32 *animVertices = nullptr;
  f32 *animNormals = nullptr;
  u32 vaoId = 0;
  u32 *vboId = nullptr;
  u8 name[64]{};
  i32 id = 0;
  i32 parentId = 0;

  constexpr Mesh() = default;
  constexpr Mesh(const ::Mesh &raw) noexcept
      : Mesh(detail::raylib_cast<Mesh>(raw)) {}

  constexpr operator ::Mesh() const noexcept {
    return detail::raylib_cast<::Mesh>(*this);
  }

  auto operator=(const ::Mesh &raw) noexcept -> Mesh & {
    *this = Mesh(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool {
    return vertexCount > 0 || triangleCount > 0 || vaoId != 0;
  }

  [[nodiscard]] auto uploaded() const -> bool { return vaoId != 0; }
  [[nodiscard]] auto vertex_count() const -> int { return vertexCount; }
  [[nodiscard]] auto vertex_data() const -> const float * { return vertices; }
  [[nodiscard]] auto vertex_data() -> float * { return vertices; }

  auto gen_tangents() -> void {
    auto raw = static_cast<::Mesh>(*this);
    rl::GenMeshTangents(&raw);
    *this = raw;
  }

  auto upload(bool dynamic) -> void {
    auto raw = static_cast<::Mesh>(*this);
    rl::UploadMesh(&raw, dynamic);
    *this = raw;
  }

  auto unload() -> void {
    if (Mesh::ready()) {
      rl::UnloadMesh(*this);
      *this = Mesh{};
    }
  }
  auto update_buffer(i32 index, const void *data, i32 data_size,
                     i32 offset) const -> void {
    rl::UpdateMeshBuffer(*this, index, data, data_size, offset);
  }

  auto draw_instanced(const Material &material, const Matrix *transforms,
                      i32 count) const -> void;
};

struct Material {
  Shader shader{};
  MaterialMap *maps{};
  f32 params[4]{};

  constexpr Material() = default;
  constexpr Material(const ::Material &raw) noexcept
      : Material(detail::raylib_cast<Material>(raw)) {}

  constexpr operator ::Material() const noexcept {
    return detail::raylib_cast<::Material>(*this);
  }

  auto operator=(const ::Material &raw) noexcept -> Material & {
    *this = Material(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool {
    return shader.ready() || maps != nullptr;
  }

  static auto fallback_material(ecs::NonSendMarker) -> gl::Material & {
    static gl::Material fallback = {};
    static bool init = false;
    if (!init) {
      fallback.shader = Shader::get_default();
      static std::vector<gl::MaterialMap> maps(ecs::kMaterialMapCount);
      fallback.maps = maps.data();
      constexpr int albedo = ecs::material_map_index(ecs::MaterialMap::Albedo);
      fallback.maps[albedo].texture.id = gl::default_texture_id();
      fallback.maps[albedo].texture.width = 1;
      fallback.maps[albedo].texture.height = 1;
      fallback.maps[albedo].texture.mipmaps = 1;
      fallback.maps[albedo].texture.format =
          gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
      fallback.maps[albedo].color = gl::kWhite;
      init = true;
    }
    return fallback;
  }
};

inline auto Mesh::draw_instanced(const Material &material,
                                 const Matrix *transforms, i32 count) const
    -> void {
  rl::DrawMeshInstanced(*this, material, transforms, count);
}

struct RenderTarget {
  u32 id{};
  Texture texture{};
  Texture depth{};

  constexpr RenderTarget() = default;
  constexpr RenderTarget(const ::RenderTexture &raw) noexcept
      : RenderTarget(detail::raylib_cast<RenderTarget>(raw)) {}

  RenderTarget(u32 width, u32 height)
      : RenderTarget(rl::LoadRenderTexture(width, height)) {}

  constexpr operator ::RenderTexture() const noexcept {
    return detail::raylib_cast<::RenderTexture>(*this);
  }

  auto operator=(const ::RenderTexture &raw) noexcept -> RenderTarget & {
    *this = RenderTarget(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id != 0; }

  auto unload() -> void {
    if (RenderTarget::ready()) {
      rl::UnloadRenderTexture(*this);
      *this = RenderTarget{};
    }
  }

  auto begin() const -> void { rl::BeginTextureMode(*this); }
  static auto end() -> void { rl::EndTextureMode(); };
};

static_assert(sizeof(Image) == sizeof(::Image));
static_assert(alignof(Image) == alignof(::Image));
static_assert(std::is_trivially_copyable_v<Image>);
static_assert(sizeof(Rectangle) == sizeof(::rlRectangle));
static_assert(alignof(Rectangle) == alignof(::rlRectangle));
static_assert(std::is_trivially_copyable_v<Rectangle>);
static_assert(sizeof(Texture) == sizeof(::Texture));
static_assert(alignof(Texture) == alignof(::Texture));
static_assert(std::is_trivially_copyable_v<Texture>);
static_assert(sizeof(Sound) == sizeof(::Sound));
static_assert(alignof(Sound) == alignof(::Sound));
static_assert(std::is_trivially_copyable_v<Sound>);
static_assert(sizeof(Shader) == sizeof(::Shader));
static_assert(alignof(Shader) == alignof(::Shader));
static_assert(std::is_trivially_copyable_v<Shader>);
static_assert(sizeof(MaterialMap) == sizeof(::MaterialMap));
static_assert(alignof(MaterialMap) == alignof(::MaterialMap));
static_assert(std::is_trivially_copyable_v<MaterialMap>);
static_assert(sizeof(Mesh) == sizeof(::Mesh));
static_assert(alignof(Mesh) == alignof(::Mesh));
static_assert(std::is_trivially_copyable_v<Mesh>);
static_assert(sizeof(Material) == sizeof(::Material));
static_assert(alignof(Material) == alignof(::Material));
static_assert(std::is_trivially_copyable_v<Material>);
static_assert(sizeof(RenderTarget) == sizeof(::RenderTexture));
static_assert(alignof(RenderTarget) == alignof(::RenderTexture));
static_assert(std::is_trivially_copyable_v<RenderTarget>);

inline auto color_to_vec3(Color color) -> Vec3 {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
}

inline auto Image::load_texture() const -> Texture {
  return rl::LoadTextureFromImage(*this);
}

} // namespace gl
