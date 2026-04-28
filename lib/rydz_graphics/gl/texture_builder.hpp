#pragma once

#include "rl.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <cmath>
#include <external/glad.h>
#include <string>
#include <string_view>

namespace gl {

enum class TextureFilter { Nearest, Linear, Bilinear, Trilinear };
enum class TextureWrap { Repeat, Clamp, MirrorRepeat, MirrorClamp };

class TextureBuilder {
public:
  static auto from_file(std::string_view path) -> TextureBuilder;
  static auto from_image(Image const& image) -> TextureBuilder;
  static auto empty(i32 width, i32 height, i32 format) -> TextureBuilder;

  auto with_filter(TextureFilter filter) -> TextureBuilder&;
  auto with_wrap(TextureWrap wrap) -> TextureBuilder&;
  auto with_mipmaps(bool generate) -> TextureBuilder&;
  auto with_anisotropic(f32 level) -> TextureBuilder&;

  auto build() -> Texture;
  auto build_or_default() -> Texture;

private:
  friend struct Texture;

  enum class SourceType { None, File, Image, Empty };

  SourceType source_type_ = SourceType::None;
  std::string file_path_;
  Image source_image_;
  i32 width_ = 0;
  i32 height_ = 0;
  i32 format_ = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

  TextureFilter filter_ = TextureFilter::Linear;
  TextureWrap wrap_ = TextureWrap::Repeat;
  bool generate_mipmaps_ = false;
  f32 anisotropic_level_ = 1.0F;
  bool is_valid_ = true;

  TextureBuilder() = default;

  auto apply_filter(Texture const& texture) const -> void;
  auto apply_wrap(Texture const& texture) const -> void;
  auto apply_mipmaps(Texture& texture) const -> void;
  auto apply_anisotropic(Texture const& texture) const -> void;
  [[nodiscard]] auto create_default_texture() const -> Texture;
};

inline auto TextureBuilder::from_file(std::string_view path) -> TextureBuilder {
  TextureBuilder builder;
  builder.source_type_ = SourceType::File;
  builder.file_path_ = std::string(path);

  if (path.empty()) {
    builder.is_valid_ = false;
    warn("TextureBuilder::from_file: empty path provided");
  }

  return builder;
}

inline auto TextureBuilder::from_image(Image const& image) -> TextureBuilder {
  TextureBuilder builder;
  builder.source_type_ = SourceType::Image;
  builder.source_image_ = image;

  if (!image.ready()) {
    builder.is_valid_ = false;
    warn("TextureBuilder::from_image: invalid image provided");
  }

  return builder;
}

inline auto TextureBuilder::empty(i32 width, i32 height, i32 format) -> TextureBuilder {
  TextureBuilder builder;
  builder.source_type_ = SourceType::Empty;
  builder.width_ = width;
  builder.height_ = height;
  builder.format_ = format;

  if (width <= 0 || height <= 0) {
    builder.is_valid_ = false;
    warn("TextureBuilder::empty: invalid dimensions ({}, {})", width, height);
  }

  return builder;
}

inline auto TextureBuilder::with_filter(TextureFilter filter) -> TextureBuilder& {
  filter_ = filter;
  return *this;
}

inline auto TextureBuilder::with_wrap(TextureWrap wrap) -> TextureBuilder& {
  wrap_ = wrap;
  return *this;
}

inline auto TextureBuilder::with_mipmaps(bool generate) -> TextureBuilder& {
  generate_mipmaps_ = generate;
  return *this;
}

inline auto TextureBuilder::with_anisotropic(f32 level) -> TextureBuilder& {
  anisotropic_level_ = level;
  return *this;
}

inline auto TextureBuilder::build() -> Texture {
  if (!is_valid_) {
    warn("TextureBuilder::build: builder is invalid, returning default texture");
    return create_default_texture();
  }

  Texture texture;

  switch (source_type_) {
  case SourceType::File: {
    texture = rl::LoadTexture(file_path_.c_str());
    if (!texture.ready()) {
      warn("TextureBuilder::build: failed to load texture from file: {}", file_path_);
      return create_default_texture();
    }
    break;
  }
  case SourceType::Image: {
    texture = rl::LoadTextureFromImage(source_image_);
    if (!texture.ready()) {
      warn("TextureBuilder::build: failed to load texture from image");
      return create_default_texture();
    }
    break;
  }
  case SourceType::Empty: {

    texture.id = rl::rlLoadTexture(nullptr, width_, height_, format_, 1);
    texture.width = width_;
    texture.height = height_;
    texture.mipmaps = 1;
    texture.format = format_;

    if (!texture.ready()) {
      warn("TextureBuilder::build: failed to create empty texture");
      return create_default_texture();
    }
    break;
  }
  case SourceType::None:
  default:
    warn("TextureBuilder::build: no source specified");
    return create_default_texture();
  }

  apply_filter(texture);
  apply_wrap(texture);

  if (generate_mipmaps_) {
    apply_mipmaps(texture);
  }

  if (anisotropic_level_ > 1.0F) {
    apply_anisotropic(texture);
  }

  return texture;
}

inline auto TextureBuilder::build_or_default() -> Texture { return build(); }

inline auto TextureBuilder::apply_filter(Texture const& texture) const -> void {
  if (!texture.ready()) {
    return;
  }

  i32 min_filter = 0;
  i32 mag_filter = 0;

  switch (filter_) {
  case TextureFilter::Nearest:
    min_filter = GL_NEAREST;
    mag_filter = GL_NEAREST;
    break;
  case TextureFilter::Linear:
    min_filter = GL_LINEAR;
    mag_filter = GL_LINEAR;
    break;
  case TextureFilter::Bilinear:
    min_filter = GL_LINEAR_MIPMAP_NEAREST;
    mag_filter = GL_LINEAR;
    break;
  case TextureFilter::Trilinear:
    min_filter = GL_LINEAR_MIPMAP_LINEAR;
    mag_filter = GL_LINEAR;
    break;
  }

  rl::SetTextureParameteri(texture.id, GL_TEXTURE_MIN_FILTER, min_filter);
  rl::SetTextureParameteri(texture.id, GL_TEXTURE_MAG_FILTER, mag_filter);
}

inline auto TextureBuilder::apply_wrap(Texture const& texture) const -> void {
  if (!texture.ready()) {
    return;
  }

  i32 wrap_mode = 0;

  switch (wrap_) {
  case TextureWrap::Repeat:
    wrap_mode = GL_REPEAT;
    break;
  case TextureWrap::Clamp:
    wrap_mode = GL_CLAMP_TO_EDGE;
    break;
  case TextureWrap::MirrorRepeat:
    wrap_mode = GL_MIRRORED_REPEAT;
    break;
  case TextureWrap::MirrorClamp:
    wrap_mode = GL_MIRROR_CLAMP_TO_EDGE;
    break;
  }

  rl::SetTextureParameteri(texture.id, GL_TEXTURE_WRAP_S, wrap_mode);
  rl::SetTextureParameteri(texture.id, GL_TEXTURE_WRAP_T, wrap_mode);
}

inline auto TextureBuilder::apply_mipmaps(Texture& texture) const -> void {
  if (!texture.ready()) {
    return;
  }

  bool const is_pot_width = (texture.width & (texture.width - 1)) == 0;
  bool const is_pot_height = (texture.height & (texture.height - 1)) == 0;

  if (!is_pot_width || !is_pot_height) {
    warn(
      "TextureBuilder::apply_mipmaps: texture dimensions ({}, {}) are not power-of-two, "
      "mipmaps may not be supported",
      texture.width,
      texture.height
    );
  }

  rl::GenTextureMipmaps(texture.id);

  i32 const max_dim = std::max(texture.width, texture.height);
  texture.mipmaps =
    static_cast<i32>(std::floor(std::log2(static_cast<f32>(max_dim)))) + 1;
}

inline auto TextureBuilder::apply_anisotropic(Texture const& texture) const -> void {
  if (!texture.ready()) {
    return;
  }

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  f32 max_anisotropy = 0.0F;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);

  if (max_anisotropy > 1.0F) {
    f32 const clamped_level = std::min(anisotropic_level_, max_anisotropy);
    rl::SetTextureParameterf(texture.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, clamped_level);
  }
#endif
}

inline auto TextureBuilder::create_default_texture() const -> Texture {
  Texture texture;
  texture.id = rl::rlGetTextureIdDefault();
  texture.width = 1;
  texture.height = 1;
  texture.mipmaps = 1;
  texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  return texture;
}

#include "rl.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <external/glad.h>

class RenderTargetBuilder {
public:
  static auto with_size(u32 width, u32 height) -> RenderTargetBuilder;

  auto with_color_format(i32 format) -> RenderTargetBuilder&;
  auto with_depth_attachment(bool enable) -> RenderTargetBuilder&;
  auto with_depth_texture(bool enable) -> RenderTargetBuilder&;
  auto with_stencil_attachment(bool enable) -> RenderTargetBuilder&;
  auto with_samples(i32 samples) -> RenderTargetBuilder&;

  auto build() -> RenderTarget;

private:
  friend struct RenderTarget;

  u32 width_ = 0;
  u32 height_ = 0;
  i32 color_format_ = RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  bool depth_attachment_ = false;
  bool depth_texture_ = false;
  bool stencil_attachment_ = false;
  i32 samples_ = 1;
  bool is_valid_ = true;

  RenderTargetBuilder() = default;

  auto create_color_attachment() -> Texture;
  auto create_depth_attachment() -> Texture;
  auto create_depth_texture_attachment() -> Texture;
  auto attach_to_framebuffer(u32 fbo_id, Texture const& color, Texture const& depth)
    -> bool;
  auto check_framebuffer_completeness(u32 fbo_id) -> bool;
  [[nodiscard]] auto create_default_render_target() const -> RenderTarget;
};

inline auto RenderTargetBuilder::with_size(u32 width, u32 height) -> RenderTargetBuilder {
  RenderTargetBuilder builder;
  builder.width_ = width;
  builder.height_ = height;

  if (width == 0 || height == 0) {
    builder.is_valid_ = false;
    warn("RenderTargetBuilder::with_size: invalid dimensions ({}, {})", width, height);
  }

  return builder;
}

inline auto RenderTargetBuilder::with_color_format(i32 format) -> RenderTargetBuilder& {
  color_format_ = format;
  return *this;
}

inline auto RenderTargetBuilder::with_depth_attachment(bool enable)
  -> RenderTargetBuilder& {
  depth_attachment_ = enable;
  return *this;
}

inline auto RenderTargetBuilder::with_depth_texture(bool enable) -> RenderTargetBuilder& {
  depth_texture_ = enable;
  return *this;
}

inline auto RenderTargetBuilder::with_stencil_attachment(bool enable)
  -> RenderTargetBuilder& {
  stencil_attachment_ = enable;
  return *this;
}

inline auto RenderTargetBuilder::with_samples(i32 samples) -> RenderTargetBuilder& {
  samples_ = samples;
  return *this;
}

inline auto RenderTargetBuilder::build() -> RenderTarget {
  if (!is_valid_) {
    warn(
      "RenderTargetBuilder::build: builder is invalid, returning default render target"
    );
    return create_default_render_target();
  }

  RenderTarget target{};

  target.id = rl::rlLoadFramebuffer();
  if (target.id == 0) {
    warn("RenderTargetBuilder::build: failed to create framebuffer");
    return create_default_render_target();
  }

  rl::BindFramebuffer(GL_FRAMEBUFFER, target.id);

  target.texture = create_color_attachment();
  if (!target.texture.ready()) {
    warn("RenderTargetBuilder::build: failed to create color attachment");
    rl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    target.unload();
    return create_default_render_target();
  }

  rl::rlFramebufferAttach(
    target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0
  );

  if (depth_texture_) {
    target.depth = create_depth_texture_attachment();
    if (!target.depth.ready()) {
      warn("RenderTargetBuilder::build: failed to create depth texture attachment");
      rl::BindFramebuffer(GL_FRAMEBUFFER, 0);
      target.unload();
      return create_default_render_target();
    }

    rl::rlFramebufferAttach(
      target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0
    );

    rl::SetTextureParameteri(target.depth.id, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  } else if (depth_attachment_) {
    target.depth = create_depth_attachment();
    if (!target.depth.ready()) {
      warn("RenderTargetBuilder::build: failed to create depth attachment");
      rl::BindFramebuffer(GL_FRAMEBUFFER, 0);
      target.unload();
      return create_default_render_target();
    }

    rl::rlFramebufferAttach(
      target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0
    );
  }

  if (!check_framebuffer_completeness(target.id)) {
    rl::BindFramebuffer(GL_FRAMEBUFFER, 0);
    target.unload();
    return create_default_render_target();
  }

  rl::BindFramebuffer(GL_FRAMEBUFFER, 0);

  return target;
}

inline auto RenderTargetBuilder::create_color_attachment() -> Texture {
  Texture texture;
  texture.id = rl::rlLoadTexture(
    nullptr, static_cast<int>(width_), static_cast<int>(height_), color_format_, 1
  );
  texture.width = static_cast<i32>(width_);
  texture.height = static_cast<i32>(height_);
  texture.mipmaps = 1;
  texture.format = color_format_;
  return texture;
}

inline auto RenderTargetBuilder::create_depth_attachment() -> Texture {
  Texture texture;
  texture.id =
    rl::rlLoadTextureDepth(static_cast<int>(width_), static_cast<int>(height_), false);
  texture.width = static_cast<i32>(width_);
  texture.height = static_cast<i32>(height_);
  texture.mipmaps = 1;
  texture.format = 19;
  return texture;
}

inline auto RenderTargetBuilder::create_depth_texture_attachment() -> Texture {
  Texture texture;
  texture.id =
    rl::rlLoadTextureDepth(static_cast<int>(width_), static_cast<int>(height_), true);
  texture.width = static_cast<i32>(width_);
  texture.height = static_cast<i32>(height_);
  texture.mipmaps = 1;
  texture.format = 19;
  return texture;
}

inline auto RenderTargetBuilder::check_framebuffer_completeness(u32 fbo_id) -> bool {
  u32 const status = rl::CheckFramebufferStatus(GL_FRAMEBUFFER);

  if (status == GL_FRAMEBUFFER_COMPLETE) {
    return true;
  }

  char const* reason = "Unknown";
  switch (status) {
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    reason = "Incomplete attachment";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    reason = "Missing attachment";
    break;
#ifdef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
    reason = "Incomplete draw buffer";
    break;
#endif
#ifdef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
    reason = "Incomplete read buffer";
    break;
#endif
  case GL_FRAMEBUFFER_UNSUPPORTED:
    reason = "Unsupported format combination";
    break;
#ifdef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE
  case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
    reason = "Incomplete multisample";
    break;
#endif
  default:
    break;
  }

  warn("RenderTargetBuilder::build: framebuffer incomplete - {}", reason);
  return false;
}

inline auto RenderTargetBuilder::create_default_render_target() const -> RenderTarget {
  return RenderTarget{};
}

} // namespace gl
