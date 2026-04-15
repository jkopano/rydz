#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_ecs/params.hpp"
#include "types.hpp"
#include <bit>
#include <external/glad.h>
#include <type_traits>
#include <utility>

namespace gl {

using Color = rl::Color;
using Vec2 = rl::Vector2;
using Vec3 = rl::Vector3;
using Vec4 = rl::Vector4;
using Matrix = rl::Matrix;
using Rectangle = rl::Rectangle;
using MaterialMapIndex = ::MaterialMapIndex;
using BoneInfo = ::BoneInfo;
using ModelAnimPose = ::ModelAnimPose;
using ModelSkeleton = ::ModelSkeleton;
using AudioStream = ::AudioStream;

inline constexpr int SHADER_UNIFORM_FLOAT = ::SHADER_UNIFORM_FLOAT;
inline constexpr int SHADER_UNIFORM_VEC2 = ::SHADER_UNIFORM_VEC2;
inline constexpr int SHADER_UNIFORM_VEC3 = ::SHADER_UNIFORM_VEC3;
inline constexpr int SHADER_UNIFORM_VEC4 = ::SHADER_UNIFORM_VEC4;
inline constexpr int SHADER_UNIFORM_INT = ::SHADER_UNIFORM_INT;
inline constexpr int SHADER_UNIFORM_IVEC2 = ::SHADER_UNIFORM_IVEC2;
inline constexpr int SHADER_UNIFORM_IVEC3 = ::SHADER_UNIFORM_IVEC3;
inline constexpr int SHADER_UNIFORM_IVEC4 = ::SHADER_UNIFORM_IVEC4;

inline constexpr int SHADER_LOC_MAP_DIFFUSE = ::SHADER_LOC_MAP_DIFFUSE;
inline constexpr int SHADER_LOC_MAP_ROUGHNESS = ::SHADER_LOC_MAP_ROUGHNESS;
inline constexpr int SHADER_LOC_MAP_OCCLUSION = ::SHADER_LOC_MAP_OCCLUSION;
inline constexpr int SHADER_LOC_MAP_EMISSION = ::SHADER_LOC_MAP_EMISSION;
inline constexpr int SHADER_LOC_VERTEX_INSTANCETRANSFORM =
    ::SHADER_LOC_VERTEX_INSTANCETRANSFORM;
inline constexpr int SHADER_LOC_MATRIX_MODEL = ::SHADER_LOC_MATRIX_MODEL;

inline constexpr int PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 =
    ::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
inline constexpr int TEXTURE_FILTER_BILINEAR = ::TEXTURE_FILTER_BILINEAR;
inline constexpr int LOG_WARNING = ::LOG_WARNING;

inline constexpr MaterialMapIndex MATERIAL_MAP_DIFFUSE = ::MATERIAL_MAP_DIFFUSE;
inline constexpr MaterialMapIndex MATERIAL_MAP_NORMAL = ::MATERIAL_MAP_NORMAL;
inline constexpr MaterialMapIndex MATERIAL_MAP_METALNESS =
    ::MATERIAL_MAP_METALNESS;
inline constexpr MaterialMapIndex MATERIAL_MAP_ROUGHNESS =
    ::MATERIAL_MAP_ROUGHNESS;
inline constexpr MaterialMapIndex MATERIAL_MAP_OCCLUSION =
    ::MATERIAL_MAP_OCCLUSION;
inline constexpr MaterialMapIndex MATERIAL_MAP_EMISSION =
    ::MATERIAL_MAP_EMISSION;

inline unsigned int default_shader_id() { return rl::rlGetShaderIdDefault(); }
inline int *default_shader_locs() { return rl::rlGetShaderLocsDefault(); }
inline unsigned int default_texture_id() { return rl::rlGetTextureIdDefault(); }

inline constexpr Color kWhite = {255, 255, 255, 255};
inline constexpr Color kBlack = {0, 0, 0, 255};

namespace detail {

template <typename To, typename From>
[[nodiscard]] constexpr To raylib_cast(const From &value) noexcept {
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
  Buffer &operator=(const Buffer &) = delete;
  Buffer(Buffer &&) noexcept = default;
  Buffer &operator=(Buffer &&) noexcept = default;

  virtual ~Buffer() = default;

  [[nodiscard]] virtual bool ready() const = 0;
  [[nodiscard]] virtual u32 id() const = 0;

  virtual void reset() = 0;
  virtual void update(const void *data, unsigned int size,
                      unsigned int offset = 0) const = 0;
  virtual void bind(unsigned int index) const = 0;
};

class SSBO final : public Buffer {
public:
  SSBO() = default;
  SSBO(unsigned int size, const void *data, int usage)
      : id_(rl::rlLoadShaderBuffer(size, data, usage)) {}

  SSBO(const SSBO &) = delete;
  SSBO &operator=(const SSBO &) = delete;
  SSBO(SSBO &&other) noexcept : id_(std::exchange(other.id_, 0)) {}
  SSBO &operator=(SSBO &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~SSBO() override { reset(); }

  [[nodiscard]] bool ready() const override { return id_ != 0; }
  [[nodiscard]] u32 id() const override { return id_; }

  void reset() override {
    if (id_ != 0) {
      rl::rlUnloadShaderBuffer(id_);
      id_ = 0;
    }
  }
  void update(const void *data, unsigned int size,
              unsigned int offset) const override {
    if (id_ != 0) {
      rl::rlUpdateShaderBuffer(id_, data, size, offset);
    }
  }

  void bind(unsigned int index) const override {
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
  UBO &operator=(const UBO &) = delete;
  UBO &operator=(UBO &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~UBO() override { reset(); }

  [[nodiscard]] bool ready() const override { return id_ != 0; }
  [[nodiscard]] u32 id() const override { return id_; }

  void reset() override {
    if (id_ != 0) {
      glDeleteBuffers(1, &id_);
      id_ = 0;
    }
  }

  void update(const void *data, unsigned int size,
              unsigned int offset) const override {
    if (id_ != 0) {
      glBindBuffer(GL_UNIFORM_BUFFER, id_);
      glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
  }

  void bind(unsigned int index) const override {
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

  static VertexArray create() {
    VertexArray array;
    array.id_ = rl::rlLoadVertexArray();
    return array;
  }

  VertexArray(const VertexArray &) = delete;
  VertexArray &operator=(const VertexArray &) = delete;
  VertexArray(VertexArray &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  VertexArray &operator=(VertexArray &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexArray() { reset(); }

  [[nodiscard]] bool ready() const { return id_ != 0; }
  [[nodiscard]] u32 id() const { return id_; }

  void reset() {
    if (id_ != 0) {
      rl::rlUnloadVertexArray(id_);
      id_ = 0;
    }
  }

  [[nodiscard]] bool bind() const {
    if (id_ == 0) {
      return false;
    }
    return rl::rlEnableVertexArray(id_);
  }

  static void unbind() { rl::rlDisableVertexArray(); }

  void draw(i32 offset, i32 count) const {
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

  static VertexBuffer create(const void *data, i32 size, bool dynamic = false) {
    return {data, size, dynamic};
  }

  VertexBuffer(const VertexBuffer &) = delete;
  VertexBuffer &operator=(const VertexBuffer &) = delete;
  VertexBuffer(VertexBuffer &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  VertexBuffer &operator=(VertexBuffer &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~VertexBuffer() { reset(); }

  [[nodiscard]] bool ready() const { return id_ != 0; }
  [[nodiscard]] u32 id() const { return id_; }

  void reset() {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  void bind() const {
    if (id_ != 0) {
      rl::rlEnableVertexBuffer(id_);
    }
  }

  static void unbind() { rl::rlDisableVertexBuffer(); }

private:
  u32 id_ = 0;
};

class ElementBuffer final {
public:
  ElementBuffer() = default;

  ElementBuffer(const void *data, i32 size, bool dynamic = false)
      : id_(rl::rlLoadVertexBufferElement(data, size, dynamic)) {}

  static ElementBuffer create(const void *data, i32 size,
                              bool dynamic = false) {
    return {data, size, dynamic};
  }

  ElementBuffer(const ElementBuffer &) = delete;
  ElementBuffer &operator=(const ElementBuffer &) = delete;
  ElementBuffer(ElementBuffer &&other) noexcept
      : id_(std::exchange(other.id_, 0)) {}
  ElementBuffer &operator=(ElementBuffer &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    id_ = std::exchange(other.id_, 0);
    return *this;
  }

  ~ElementBuffer() { reset(); }

  [[nodiscard]] bool ready() const { return id_ != 0; }
  [[nodiscard]] u32 id() const { return id_; }

  void reset() {
    if (id_ != 0) {
      rl::rlUnloadVertexBuffer(id_);
      id_ = 0;
    }
  }

  void bind() const {
    if (id_ != 0) {
      rl::rlEnableVertexBufferElement(id_);
    }
  }

  static void unbind() { rl::rlDisableVertexBufferElement(); }

  static void draw(i32 offset, i32 count, const void *buffer = nullptr) {
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

  Image &operator=(const ::Image &raw) noexcept {
    *this = Image(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const { return data != nullptr; }

  void unload() {
    if (ready()) {
      rl::UnloadImage(*this);
      *this = Image{};
    }
  }

  void format_to(int pixel_format) {
    auto raw = static_cast<::Image>(*this);
    rl::ImageFormat(&raw, pixel_format);
    *this = raw;
  }

  [[nodiscard]] Texture load_texture() const;
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

  Texture &operator=(const ::Texture &raw) noexcept {
    *this = Texture(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const { return id > 0; }

  void unload() {
    if (ready()) {
      rl::UnloadTexture(*this);
      *this = Texture{};
    }
  }

  void set_filter(i32 filter) const { rl::SetTextureFilter(*this, filter); }

  [[nodiscard]] Rectangle rect() const {
    return {0.0F, 0.0F, static_cast<f32>(width), static_cast<float>(height)};
  }

  [[nodiscard]] Rectangle flipped_rect() const {
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

  Sound &operator=(const ::Sound &raw) noexcept {
    *this = Sound(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const {
    return stream.buffer != nullptr && frameCount > 0;
  }

  void unload() {
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

  Shader &operator=(const ::Shader &raw) noexcept {
    *this = Shader(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const { return id != 0; }
  [[nodiscard]] bool has_locations() const { return locs != nullptr; }

  i32 uniform_location(const char *name) const {
    return rl::GetShaderLocation(*this, name);
  }

  i32 attribute_location(const char *name) const {
    return rl::GetShaderLocationAttrib(*this, name);
  }

  static Shader get_default() {
    Shader shader = {};
    shader.id = default_id();
    shader.locs = default_locs();
    return shader;
  }
  static u32 default_id() { return rl::rlGetShaderIdDefault(); }
  static i32 *default_locs() { return rl::rlGetShaderLocsDefault(); }
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

  MaterialMap &operator=(const ::MaterialMap &raw) noexcept {
    *this = MaterialMap(raw);
    return *this;
  }

  [[nodiscard]] bool has_texture() const { return texture.ready(); }
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

  Mesh &operator=(const ::Mesh &raw) noexcept {
    *this = Mesh(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const {
    return vertexCount > 0 || triangleCount > 0 || vaoId != 0;
  }

  [[nodiscard]] bool uploaded() const { return vaoId != 0; }
  [[nodiscard]] int vertex_count() const { return vertexCount; }
  [[nodiscard]] const float *vertex_data() const { return vertices; }
  [[nodiscard]] float *vertex_data() { return vertices; }

  void gen_tangents() {
    auto raw = static_cast<::Mesh>(*this);
    rl::GenMeshTangents(&raw);
    *this = raw;
  }

  void upload(bool dynamic) {
    auto raw = static_cast<::Mesh>(*this);
    rl::UploadMesh(&raw, dynamic);
    *this = raw;
  }

  void unload() {
    if (Mesh::ready()) {
      rl::UnloadMesh(*this);
      *this = Mesh{};
    }
  }
  void update_buffer(i32 index, const void *data, i32 data_size,
                     i32 offset) const {
    rl::UpdateMeshBuffer(*this, index, data, data_size, offset);
  }
  void draw_instanced(Material &material, const Matrix *transforms,
                      i32 count) const;
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

  Material &operator=(const ::Material &raw) noexcept {
    *this = Material(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const { return shader.ready() || maps != nullptr; }

  static gl::Material &fallback_material(ecs::NonSendMarker) {
    static gl::Material fallback = {};
    static bool init = false;
    if (!init) {
      fallback.shader = Shader::get_default();
      static std::vector<gl::MaterialMap> maps(12);
      fallback.maps = maps.data();
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.id =
          gl::default_texture_id();
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.width = 1;
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.height = 1;
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.mipmaps = 1;
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].texture.format =
          gl::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
      fallback.maps[gl::MATERIAL_MAP_DIFFUSE].color = gl::kWhite;
      init = true;
    }
    return fallback;
  }
};

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

  RenderTarget &operator=(const ::RenderTexture &raw) noexcept {
    *this = RenderTarget(raw);
    return *this;
  }

  [[nodiscard]] bool ready() const { return id != 0; }

  void unload() {
    if (RenderTarget::ready()) {
      rl::UnloadRenderTexture(*this);
      *this = RenderTarget{};
    }
  }

  void begin() const { rl::BeginTextureMode(*this); }
  static void end() { rl::EndTextureMode(); };
};

static_assert(sizeof(Image) == sizeof(::Image));
static_assert(alignof(Image) == alignof(::Image));
static_assert(std::is_trivially_copyable_v<Image>);
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

inline Vec3 color_to_vec3(Color color) {
  return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
}

template <typename... Args>
inline void trace_log(int level, const char *format, Args... args) {
  rl::TraceLog(level, format, args...);
}

inline Texture Image::load_texture() const {
  return rl::LoadTextureFromImage(*this);
}

inline void Mesh::draw_instanced(Material &material, const Matrix *transforms,
                                 i32 count) const {
  rl::DrawMeshInstanced(*this, material, transforms, count);
}

} // namespace gl
