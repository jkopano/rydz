#pragma once

#include "rl.hpp"
#include "rydz_graphics/components/color.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_log/mod.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <external/glad.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace gl {

struct Texture;
class TextureBuilder;
class RenderTargetBuilder;

struct Image {
  void* data{};
  i32 width{};
  i32 height{};
  i32 mipmaps{};
  i32 format{};

  constexpr Image() = default;
  constexpr Image(::Image const& raw) noexcept : Image(detail::raylib_cast<Image>(raw)) {}

  constexpr operator ::Image() const noexcept {
    return detail::raylib_cast<::Image>(*this);
  }

  auto operator=(::Image const& raw) noexcept -> Image& {
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

  auto format_to(i32 pixel_format) -> void {
    auto raw = static_cast<::Image>(*this);
    rl::ImageFormat(&raw, pixel_format);
    *this = raw;
  }

  [[nodiscard]] auto load_texture() const -> Texture;
};

static_assert(sizeof(Image) == sizeof(::Image));
static_assert(alignof(Image) == alignof(::Image));
static_assert(std::is_trivially_copyable_v<Image>);

struct Texture {
  u32 id = 0;
  i32 width = 0;
  i32 height = 0;
  i32 mipmaps = 0;
  i32 format = 0;

public:
  constexpr Texture() = default;

  constexpr Texture(::rlTexture const& raw) noexcept
      : id(raw.id), width(raw.width), height(raw.height), mipmaps(raw.mipmaps),
        format(raw.format) {}
  constexpr Texture(char const* path) { *this = rl::LoadTexture(path); }
  constexpr Texture(std::string const& path) : Texture(path.c_str()) {}

  constexpr operator ::rlTexture() const noexcept {
    return ::rlTexture{id, width, height, mipmaps, format};
  }

  auto operator=(::rlTexture const& raw) noexcept -> Texture& {
    *this = Texture(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id > 0; }

  /// Example usage:
  /// ```cpp
  /// auto texture = Texture::builder()
  ///   .from_file("path/to/texture.png")
  ///   .with_filter(TextureFilter::Trilinear)
  ///   .with_wrap(TextureWrap::Repeat)
  ///   .with_mipmaps(true)
  ///   .build();
  /// ```
  ///
  /// @return A default-constructed TextureBuilder ready for configuration
  static auto builder() -> TextureBuilder;

  auto unload() -> void {
    if (ready()) {
      rl::UnloadTexture(*this);
      id = 0;
      width = 0;
      height = 0;
      mipmaps = 0;
      format = 0;
    }
  }

  auto set_filter(i32 filter) const -> void {
    if (!ready()) {
      return;
    }
    rl::SetTextureParameteri(id, GL_TEXTURE_MIN_FILTER, filter);
    rl::SetTextureParameteri(id, GL_TEXTURE_MAG_FILTER, filter);
  }

  auto set_wrap(i32 wrap) const -> void {
    if (!ready()) {
      return;
    }
    rl::SetTextureParameteri(id, GL_TEXTURE_WRAP_S, wrap);
    rl::SetTextureParameteri(id, GL_TEXTURE_WRAP_T, wrap);
  }

  auto generate_mipmaps() -> void {
    if (!ready()) {
      return;
    }
    rl::GenTextureMipmaps(id);

    i32 const max_dim = std::max(width, height);
    mipmaps = static_cast<i32>(std::floor(std::log2(static_cast<f32>(max_dim)))) + 1;
  }

  [[nodiscard]] auto rect() const -> Rectangle {
    return {0.0F, 0.0F, static_cast<f32>(width), static_cast<float>(height)};
  }

  [[nodiscard]] auto flipped_rect() const -> Rectangle {
    return {0.0F, 0.0F, static_cast<f32>(width), -static_cast<float>(height)};
  }

  auto bind(i32 slot) const -> void {
    rl::rlActiveTextureSlot(slot);
    rl::rlEnableTexture(id);
  }

  auto unbind() const -> void { rl::rlDisableTexture(); }
};

static_assert(sizeof(Texture) == sizeof(::rlTexture));
static_assert(alignof(Texture) == alignof(::rlTexture));

inline auto Image::load_texture() const -> Texture {
  return rl::LoadTextureFromImage(*this);
}

struct RenderTarget {
  u32 id{};
  Texture texture{};
  Texture depth{};

public:
  constexpr RenderTarget() = default;
  constexpr RenderTarget(::RenderTexture const& raw) noexcept
      : id(raw.id), texture(raw.texture), depth(raw.depth) {}

  RenderTarget(u32 width, u32 height)
      : RenderTarget(rl::LoadRenderTexture(width, height)) {}

  RenderTarget(RenderTarget const&) = delete;
  auto operator=(RenderTarget const&) -> RenderTarget& = delete;

  RenderTarget(RenderTarget&& o) noexcept
      : id(std::exchange(o.id, 0)), texture(std::move(o.texture)),
        depth(std::move(o.depth)) {}

  auto operator=(RenderTarget&& o) noexcept -> RenderTarget& {
    if (this != &o) {
      unload();
      id = std::exchange(o.id, 0);
      texture = std::move(o.texture);
      depth = std::move(o.depth);
    }
    return *this;
  }

  ~RenderTarget() { unload(); }

  /// Returns a RenderTargetBuilder for fluent API construction
  ///
  /// Example usage:
  /// ```cpp
  /// auto render_target = RenderTarget::builder()
  ///   .with_size(1920, 1080)
  ///   .with_color_format(RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
  ///   .with_depth_texture(true)
  ///   .build();
  /// ```
  ///
  /// @return A default-constructed RenderTargetBuilder ready for configuration
  static auto builder() -> RenderTargetBuilder;

  static auto with_depth_texture(
    u32 width, u32 height, i32 color_format = RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  ) -> RenderTarget {
    RenderTarget target{};

    target.id = rl::rlLoadFramebuffer();
    rl::rlBindFramebuffer(GL_FRAMEBUFFER, target.id);

    target.texture.id = rl::rlLoadTexture(
      nullptr, static_cast<int>(width), static_cast<int>(height), color_format, 1
    );
    target.texture.width = static_cast<i32>(width);
    target.texture.height = static_cast<i32>(height);
    target.texture.mipmaps = 1;
    target.texture.format = color_format;

    rl::rlFramebufferAttach(
      target.id,
      target.texture.id,
      RL_ATTACHMENT_COLOR_CHANNEL0,
      RL_ATTACHMENT_TEXTURE2D,
      0
    );

    target.depth.id =
      rl::rlLoadTextureDepth(static_cast<int>(width), static_cast<int>(height), false);
    target.depth.width = static_cast<i32>(width);
    target.depth.height = static_cast<i32>(height);
    target.depth.mipmaps = 1;
    target.depth.format = 19;

    rl::rlFramebufferAttach(
      target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0
    );

    rl::SetTextureParameteri(target.depth.id, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    if (!rl::rlFramebufferComplete(target.id)) {
      warn("RenderTarget::with_depth_texture: FBO incomplete");
    }

    rl::rlBindFramebuffer(GL_FRAMEBUFFER, 0);

    return target;
  }

  constexpr operator ::RenderTexture() const noexcept {
    return ::RenderTexture{
      id, static_cast<::rlTexture>(texture), static_cast<::rlTexture>(depth)
    };
  }

  auto operator=(::RenderTexture const& raw) noexcept -> RenderTarget& {
    *this = RenderTarget(raw);
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id != 0; }

  [[nodiscard]] auto is_complete() const -> bool {
    if (!ready()) {
      return false;
    }

    u32 const prev_fbo = 0;
    rl::BindFramebuffer(GL_FRAMEBUFFER, id);
    u32 const status = rl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    rl::BindFramebuffer(GL_FRAMEBUFFER, prev_fbo);

    return status == GL_FRAMEBUFFER_COMPLETE;
  }

  auto unload() -> void {
    if (ready()) {
      rl::UnloadRenderTexture(*this);
      id = 0;
      texture = Texture{};
      depth = Texture{};
    }
  }

  auto begin() const -> void {
    if (!ready()) {
      return;
    }
    rl::BeginTextureMode(*this);
  }

  auto end() const -> void {
    if (!ready()) {
      return;
    }
    rl::EndTextureMode();
  }

  auto clear(ecs::Color color) const -> void {
    if (!ready()) {
      return;
    }
    begin();
    rl::ClearBackground(color);
    end();
  }

  auto resize(u32 w, u32 h) -> void {
    if (!ready()) {
      warn("RenderTarget::resize: cannot resize invalid render target");
      return;
    }
    unload();
    *this = RenderTarget(w, h);
  }
};

static_assert(sizeof(RenderTarget) == sizeof(::RenderTexture));
static_assert(alignof(RenderTarget) == alignof(::RenderTexture));

class CubeMap {
  static constexpr u32 FACE_COUNT = 6;

private:
  u32 id_ = 0;
  explicit CubeMap(u32 id) noexcept : id_(id) {}

public:
  CubeMap() = default;

  CubeMap(CubeMap const&) = delete;
  CubeMap(CubeMap&& o) noexcept : id_(std::exchange(o.id_, 0)) {}
  ~CubeMap() { unload(); }
  auto operator=(CubeMap const&) -> CubeMap& = delete;
  auto operator=(CubeMap&& o) noexcept -> CubeMap& {
    if (this != &o) {
      unload();
      id_ = std::exchange(o.id_, 0);
    }
    return *this;
  }

  [[nodiscard]] auto ready() const -> bool { return id_ != 0; }
  [[nodiscard]] auto id() const -> u32 { return id_; }

  auto bind(i32 slot = 0) const -> void {
    if (!ready()) {
      return;
    }
    rl::rlActiveTextureSlot(slot);
    rl::rlEnableTextureCubemap(id_);
  }

  auto unbind() const -> void {
    if (!ready()) {
      return;
    }
    rl::rlDisableTextureCubemap();
  }

  auto unload() -> void {
    if (id_ != 0) {
      rl::rlUnloadTexture(id_);
      id_ = 0;
    }
  }

  static auto load(std::array<std::string, 6> const& face_paths) -> CubeMap {
    std::array<Image, FACE_COUNT> images;

    for (usize i = 0; i < FACE_COUNT; ++i) {
      images.at(i) = rl::rlLoadImage(face_paths.at(i).c_str());
      if (images.at(i).data == nullptr) {
        for (usize j = 0; j < i; ++j) {
          images.at(j).unload();
        }
        return CubeMap{};
      }
      if (images.at(i).format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        images.at(i).format_to(PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      }
    }

    u32 const width = static_cast<u32>(images[0].width);
    u32 const height = static_cast<u32>(images[0].height);
    constexpr u32 pixel_size = 4U;

    std::vector<u8> data(width * height * FACE_COUNT * pixel_size);

    for (usize i = 0; i < FACE_COUNT; ++i) {
      u32 const size = width * height * pixel_size;
      usize const offset = i * size;
      std::memcpy(
        data.data() + offset,
        images.at(i).data,
        static_cast<usize>(width * height) * pixel_size
      );
      images.at(i).unload();
    }

    u32 const id = rl::rlLoadTextureCubemap(
      data.data(), static_cast<i32>(width), PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1
    );

    return CubeMap{id};
  }
  static auto from_directory(std::string const& dir, std::string const& ext) -> CubeMap {
    return CubeMap::load({
      dir + "/right" + ext,
      dir + "/left" + ext,
      dir + "/top" + ext,
      dir + "/bottom" + ext,
      dir + "/front" + ext,
      dir + "/back" + ext,
    });
  }
};

} // namespace gl

#include "rydz_graphics/gl/render_target_builder.hpp"
#include "rydz_graphics/gl/texture_builder.hpp"

namespace gl {

inline auto Texture::builder() -> TextureBuilder { return TextureBuilder{}; }
inline auto RenderTarget::builder() -> RenderTargetBuilder {
  return RenderTargetBuilder{};
}

} // namespace gl
