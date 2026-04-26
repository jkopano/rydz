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

} // namespace gl
