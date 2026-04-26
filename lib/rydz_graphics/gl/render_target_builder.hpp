#pragma once

#include "rl.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <external/glad.h>

namespace gl {

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
  auto create_default_render_target() const -> RenderTarget;
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
