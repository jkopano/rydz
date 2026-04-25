#pragma once

#include "external/raylib/src/rlgl.h"
#include "rydz_graphics/gl/primitives.hpp"
#include <array>
#include <cstring>
#include <external/glad.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace gl {

struct Texture;

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

  constexpr Texture() = default;

  constexpr Texture(::rlTexture const& raw) noexcept
      : Texture(detail::raylib_cast<Texture>(raw)) {}
  constexpr Texture(char const* path) { *this = rl::LoadTexture(path); }
  constexpr Texture(std::string const& path) : Texture(path.c_str()) {}

  constexpr operator ::rlTexture() const noexcept {
    return detail::raylib_cast<::rlTexture>(*this);
  }

  auto operator=(::rlTexture const& raw) noexcept -> Texture& {
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

  auto set_filter(i32 filter) const -> void { rl::SetTextureFilter(*this, filter); }

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
static_assert(std::is_trivially_copyable_v<Texture>);

inline auto Image::load_texture() const -> Texture {
  return rl::LoadTextureFromImage(*this);
}

struct RenderTarget {
  u32 id{};
  Texture texture{};
  Texture depth{};

  constexpr RenderTarget() = default;
  constexpr RenderTarget(::RenderTexture const& raw) noexcept
      : RenderTarget(detail::raylib_cast<RenderTarget>(raw)) {}

  RenderTarget(u32 width, u32 height)
      : RenderTarget(rl::LoadRenderTexture(width, height)) {}

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

    // Attach color texture
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

    // Attach depth texture
    rl::rlFramebufferAttach(
      target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0
    );

    rl::rlEnableTexture(target.depth.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    rl::rlDisableTexture();

    if (!rl::rlFramebufferComplete(target.id)) {
      warn("RenderTarget::with_depth_texture: FBO incomplete");
    }

    rl::rlBindFramebuffer(GL_FRAMEBUFFER, 0);

    return target;
  }

  constexpr operator ::RenderTexture() const noexcept {
    return detail::raylib_cast<::RenderTexture>(*this);
  }

  auto operator=(::RenderTexture const& raw) noexcept -> RenderTarget& {
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
  auto end() const -> void { rl::EndTextureMode(); };

  auto resize(u32 w, u32 h) -> void {
    unload();
    *this = RenderTarget(w, h);
  }
};

static_assert(sizeof(RenderTarget) == sizeof(::RenderTexture));
static_assert(alignof(RenderTarget) == alignof(::RenderTexture));
static_assert(std::is_trivially_copyable_v<RenderTarget>);

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
