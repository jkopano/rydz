#pragma once

#include "math.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/mod.hpp"
#include "rydz_graphics/extract/data.hpp"
#include "rydz_graphics/gl/buffers.hpp"
#include "rydz_graphics/gl/shader.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <external/glad.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace ecs {

inline constexpr i32 MAX_DIRECTIONAL_CASCADES = 4;
inline constexpr i32 MAX_POINT_SHADOWS = 4;
inline constexpr i32 POINT_SHADOW_FACE_COUNT = 6;
inline constexpr unsigned int SHADOW_UNIFORM_BINDING = 1;
inline constexpr std::string_view SHADOW_UNIFORM_BLOCK_NAME = "ShadowUniforms";
inline constexpr i32 SHADOW_ATLAS_TEXTURE_SLOT = 11;
inline constexpr i32 POINT_SHADOW_TEXTURE_SLOT_BASE = 12;

struct ShadowSettings {
  using T = Resource;

  bool directional_enabled = true;
  i32 cascade_count = MAX_DIRECTIONAL_CASCADES;
  u32 cascade_resolution = 2048;
  f32 cascade_split_lambda = 0.7F;
  f32 cascade_max_distance = 150.0F;
  bool point_shadows_enabled = true;
  u32 max_shadowed_point_lights = MAX_POINT_SHADOWS;
  u32 point_shadow_resolution = 1024;
  f32 directional_constant_bias = 0.0015F;
  f32 directional_normal_bias = 0.01F;
  f32 point_constant_bias = 0.01F;
  i32 directional_pcf_radius = 1;

  [[nodiscard]] auto cascade_count_clamped() const -> i32 {
    return std::clamp(cascade_count, 1, MAX_DIRECTIONAL_CASCADES);
  }

  [[nodiscard]] auto max_shadowed_point_lights_clamped() const -> i32 {
    return static_cast<i32>(
      std::min<u32>(std::max<u32>(max_shadowed_point_lights, 1U), MAX_POINT_SHADOWS)
    );
  }
};

struct ShadowView {
  Mat4 view = Mat4::IDENTITY;
  Mat4 projection = Mat4::IDENTITY;
  Mat4 view_projection = Mat4::IDENTITY;
  Vec3 position = Vec3::ZERO;
  bool orthographic = false;
  f32 near_plane = 0.1F;
  f32 far_plane = 1000.0F;

  auto update_view_projection() -> void { view_projection = projection * view; }
};

struct ExtractedShadows {
  using T = Resource;

  struct DirectionalCascade {
    ShadowView shadow_view{};
    f32 split_distance = 0.0F;
  };

  struct PointShadow {
    usize light_index = 0;
    Vec3 position = Vec3::ZERO;
    f32 near_plane = 0.1F;
    f32 far_plane = 1.0F;
    std::array<ShadowView, POINT_SHADOW_FACE_COUNT> faces{};
  };

  bool has_directional = false;
  i32 cascade_count = 0;
  std::array<DirectionalCascade, MAX_DIRECTIONAL_CASCADES> directional_cascades{};
  std::vector<PointShadow> point_shadows{};

  auto clear() -> void {
    has_directional = false;
    cascade_count = 0;
    point_shadows.clear();
    for (auto& cascade : directional_cascades) {
      cascade = {};
    }
  }
};

struct ShadowAtlasTile {
  i32 x = 0;
  i32 y = 0;
  i32 width = 1;
  i32 height = 1;
  Vec4 uv_rect = {0.0F, 0.0F, 1.0F, 1.0F};
};

inline auto shadow_face_targets() -> std::array<Vec3, POINT_SHADOW_FACE_COUNT> {
  return {
    Vec3{1.0F, 0.0F, 0.0F},
    Vec3{-1.0F, 0.0F, 0.0F},
    Vec3{0.0F, 1.0F, 0.0F},
    Vec3{0.0F, -1.0F, 0.0F},
    Vec3{0.0F, 0.0F, 1.0F},
    Vec3{0.0F, 0.0F, -1.0F},
  };
}

inline auto shadow_face_ups() -> std::array<Vec3, POINT_SHADOW_FACE_COUNT> {
  return {
    Vec3{0.0F, -1.0F, 0.0F},
    Vec3{0.0F, -1.0F, 0.0F},
    Vec3{0.0F, 0.0F, 1.0F},
    Vec3{0.0F, 0.0F, -1.0F},
    Vec3{0.0F, -1.0F, 0.0F},
    Vec3{0.0F, -1.0F, 0.0F},
  };
}

inline auto compute_cascade_splits(
  f32 near_plane, f32 far_plane, i32 cascade_count, f32 lambda
) -> std::array<f32, MAX_DIRECTIONAL_CASCADES> {
  std::array<f32, MAX_DIRECTIONAL_CASCADES> splits{};
  f32 const clamped_near = std::max(near_plane, 0.001F);
  f32 const clamped_far = std::max(far_plane, clamped_near + 0.001F);
  f32 const ratio = clamped_far / clamped_near;

  for (i32 cascade_index = 0; cascade_index < cascade_count; ++cascade_index) {
    f32 const p =
      static_cast<f32>(cascade_index + 1) / static_cast<f32>(cascade_count);
    f32 const log_split = clamped_near * std::pow(ratio, p);
    f32 const uniform_split = clamped_near + ((clamped_far - clamped_near) * p);
    splits[cascade_index] =
      (lambda * log_split) + ((1.0F - lambda) * uniform_split);
  }

  return splits;
}

inline auto compute_frustum_slice_corners_world(
  ExtractedView const& view, f32 slice_near, f32 slice_far
) -> std::array<Vec3, 8> {
  f32 const aspect =
    compute_camera_aspect_ratio(view.viewport.width, view.viewport.height);
  Mat4 const inverse_view = view.camera_view.view.inverse();

  if (view.orthographic) {
    f32 const full_half_width =
      1.0F / std::max(std::abs(view.camera_view.proj(0, 0)), 0.0001F);
    f32 const full_half_height =
      1.0F / std::max(std::abs(view.camera_view.proj(1, 1)), 0.0001F);

    std::array<Vec3, 8> view_space = {
      Vec3{-full_half_width, -full_half_height, -slice_near},
      Vec3{full_half_width, -full_half_height, -slice_near},
      Vec3{full_half_width, full_half_height, -slice_near},
      Vec3{-full_half_width, full_half_height, -slice_near},
      Vec3{-full_half_width, -full_half_height, -slice_far},
      Vec3{full_half_width, -full_half_height, -slice_far},
      Vec3{full_half_width, full_half_height, -slice_far},
      Vec3{-full_half_width, full_half_height, -slice_far},
    };

    std::array<Vec3, 8> world{};
    for (usize i = 0; i < view_space.size(); ++i) {
      world[i] = inverse_view * view_space[i];
    }
    return world;
  }

  f32 const projection_y_scale = std::max(std::abs(view.camera_view.proj(1, 1)), 0.0001F);
  f32 const tan_fov_y = 1.0F / projection_y_scale;
  f32 const tan_fov_x = tan_fov_y * aspect;

  f32 const near_height = slice_near * tan_fov_y;
  f32 const near_width = slice_near * tan_fov_x;
  f32 const far_height = slice_far * tan_fov_y;
  f32 const far_width = slice_far * tan_fov_x;

  std::array<Vec3, 8> view_space = {
    Vec3{-near_width, -near_height, -slice_near},
    Vec3{near_width, -near_height, -slice_near},
    Vec3{near_width, near_height, -slice_near},
    Vec3{-near_width, near_height, -slice_near},
    Vec3{-far_width, -far_height, -slice_far},
    Vec3{far_width, -far_height, -slice_far},
    Vec3{far_width, far_height, -slice_far},
    Vec3{-far_width, far_height, -slice_far},
  };

  std::array<Vec3, 8> world{};
  for (usize i = 0; i < view_space.size(); ++i) {
    world[i] = inverse_view * view_space[i];
  }
  return world;
}

inline auto build_directional_shadow_view(
  std::array<Vec3, 8> const& frustum_corners,
  Vec3 light_direction,
  u32 resolution
) -> ShadowView {
  light_direction = light_direction.normalized();
  if (light_direction.length_sq() <= 1e-8F) {
    light_direction = Vec3{-0.3F, -1.0F, -0.5F}.normalized();
  }

  Vec3 frustum_center = Vec3::ZERO;
  for (auto const& corner : frustum_corners) {
    frustum_center = frustum_center + corner;
  }
  frustum_center = frustum_center / static_cast<f32>(frustum_corners.size());

  Vec3 up = std::abs(light_direction.dot(Vec3{0.0F, 1.0F, 0.0F})) > 0.99F
              ? Vec3{0.0F, 0.0F, 1.0F}
              : Vec3{0.0F, 1.0F, 0.0F};

  ShadowView result{};
  result.position = frustum_center - (light_direction * 100.0F);
  result.orthographic = true;
  result.view = Mat4::look_at_rh(result.position, frustum_center, up);

  Vec3 min_bounds = Vec3::splat(std::numeric_limits<f32>::max());
  Vec3 max_bounds = Vec3::splat(-std::numeric_limits<f32>::max());
  for (auto const& corner : frustum_corners) {
    Vec3 const light_space = result.view * corner;
    min_bounds = Vec3(
      glm::min(
        static_cast<Vec3::Base const&>(min_bounds),
        static_cast<Vec3::Base const&>(light_space)
      )
    );
    max_bounds = Vec3(
      glm::max(
        static_cast<Vec3::Base const&>(max_bounds),
        static_cast<Vec3::Base const&>(light_space)
      )
    );
  }

  Vec3 center = (min_bounds + max_bounds) * 0.5F;
  f32 const width = std::max(max_bounds.x - min_bounds.x, 0.001F);
  f32 const height = std::max(max_bounds.y - min_bounds.y, 0.001F);
  f32 const texel_size_x = width / static_cast<f32>(std::max<u32>(resolution, 1U));
  f32 const texel_size_y = height / static_cast<f32>(std::max<u32>(resolution, 1U));

  if (texel_size_x > 0.0F) {
    center.x = std::floor(center.x / texel_size_x) * texel_size_x;
  }
  if (texel_size_y > 0.0F) {
    center.y = std::floor(center.y / texel_size_y) * texel_size_y;
  }

  min_bounds.x = center.x - (width * 0.5F);
  max_bounds.x = center.x + (width * 0.5F);
  min_bounds.y = center.y - (height * 0.5F);
  max_bounds.y = center.y + (height * 0.5F);

  f32 const depth_padding = std::max((max_bounds.z - min_bounds.z) * 0.25F, 25.0F);
  result.near_plane = -max_bounds.z - depth_padding;
  result.far_plane = -min_bounds.z + depth_padding;
  result.projection = Mat4::orthographic_rh(
    min_bounds.x,
    max_bounds.x,
    min_bounds.y,
    max_bounds.y,
    result.near_plane,
    result.far_plane
  );
  result.update_view_projection();
  return result;
}

inline auto build_point_shadow_views(
  Vec3 light_position, f32 near_plane, f32 far_plane
) -> std::array<ShadowView, POINT_SHADOW_FACE_COUNT> {
  std::array<ShadowView, POINT_SHADOW_FACE_COUNT> faces{};
  auto const targets = shadow_face_targets();
  auto const ups = shadow_face_ups();
  Mat4 const projection = Mat4::perspective_rh(PI / 2.0F, 1.0F, near_plane, far_plane);

  for (usize face_index = 0; face_index < faces.size(); ++face_index) {
    auto& face = faces[face_index];
    face.position = light_position;
    face.orthographic = false;
    face.near_plane = near_plane;
    face.far_plane = far_plane;
    face.view = Mat4::look_at_rh(light_position, light_position + targets[face_index], ups[face_index]);
    face.projection = projection;
    face.update_view_projection();
  }

  return faces;
}

struct alignas(16) ShadowUniformData {
  std::array<Mat4, MAX_DIRECTIONAL_CASCADES> directional_matrices{};
  Vec4 cascade_splits = Vec4::ZERO;
  std::array<Vec4, MAX_DIRECTIONAL_CASCADES> cascade_uv_rects{};
  std::array<int, 4> flags = {0, 0, 0, 0};
  Vec4 params = Vec4::ZERO;
};

static_assert(sizeof(ShadowUniformData) % 16 == 0);

struct ShadowUniformState {
  using T = Resource;

  gl::UBO buffer{};
  ShadowUniformData data{};

  auto update(ExtractedShadows const& shadows, ShadowSettings const& settings) -> void {
    for (i32 cascade_index = 0; cascade_index < MAX_DIRECTIONAL_CASCADES; ++cascade_index) {
      i32 const tile_x = cascade_index % 2;
      i32 const tile_y = cascade_index / 2;
      data.cascade_uv_rects[cascade_index] = Vec4{
        static_cast<f32>(tile_x) * 0.5F,
        static_cast<f32>(tile_y) * 0.5F,
        0.5F,
        0.5F,
      };

      if (cascade_index < shadows.cascade_count) {
        data.directional_matrices[cascade_index] =
          shadows.directional_cascades[cascade_index].shadow_view.view_projection;
        data.cascade_splits[cascade_index] =
          shadows.directional_cascades[cascade_index].split_distance;
      } else {
        data.directional_matrices[cascade_index] = Mat4::IDENTITY;
        data.cascade_splits[cascade_index] = 0.0F;
      }
    }

    data.flags = {
      shadows.has_directional ? 1 : 0,
      shadows.cascade_count,
      static_cast<int>(shadows.point_shadows.size()),
      std::max(settings.directional_pcf_radius, 0),
    };
    data.params = Vec4{
      settings.directional_constant_bias,
      settings.directional_normal_bias,
      settings.point_constant_bias,
      0.0F,
    };

    if (!buffer.ready()) {
      buffer = gl::UBO(
        static_cast<unsigned int>(sizeof(ShadowUniformData)), &data, RL_DYNAMIC_DRAW
      );
    } else {
      buffer.update(&data, static_cast<unsigned int>(sizeof(ShadowUniformData)), 0);
    }
  }

  auto bind() const -> void {
    if (buffer.ready()) {
      buffer.bind(SHADOW_UNIFORM_BINDING);
    }
  }

  auto bind_shader(gl::ShaderProgram& shader) const -> bool {
    if (!buffer.ready()) {
      return false;
    }
    bind();
    return shader.bind_uniform_block(SHADOW_UNIFORM_BLOCK_NAME, SHADOW_UNIFORM_BINDING);
  }
};

} // namespace ecs

namespace gl {

class DepthAtlas {
public:
  DepthAtlas() = default;
  DepthAtlas(DepthAtlas const&) = delete;
  auto operator=(DepthAtlas const&) -> DepthAtlas& = delete;

  DepthAtlas(DepthAtlas&& other) noexcept
      : framebuffer_(std::exchange(other.framebuffer_, 0)),
        depth_texture_(std::exchange(other.depth_texture_, {})),
        width_(std::exchange(other.width_, 0)),
        height_(std::exchange(other.height_, 0)) {}

  auto operator=(DepthAtlas&& other) noexcept -> DepthAtlas& {
    if (this == &other) {
      return *this;
    }
    unload();
    framebuffer_ = std::exchange(other.framebuffer_, 0);
    depth_texture_ = std::exchange(other.depth_texture_, {});
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    return *this;
  }

  ~DepthAtlas() { unload(); }

  [[nodiscard]] auto ready() const -> bool {
    return framebuffer_ != 0 && depth_texture_.id != 0;
  }

  [[nodiscard]] auto texture() const -> Texture const& { return depth_texture_; }
  [[nodiscard]] auto width() const -> u32 { return width_; }
  [[nodiscard]] auto height() const -> u32 { return height_; }

  auto ensure(u32 width, u32 height) -> void {
    if (ready() && width_ == width && height_ == height) {
      return;
    }

    unload();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &depth_texture_.id);
    glBindTexture(GL_TEXTURE_2D, depth_texture_.id);
    glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_DEPTH_COMPONENT32F,
      static_cast<GLsizei>(width),
      static_cast<GLsizei>(height),
      0,
      GL_DEPTH_COMPONENT,
      GL_FLOAT,
      nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float const border_color[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

    glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_.id, 0
    );
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depth_texture_.width = static_cast<i32>(width);
    depth_texture_.height = static_cast<i32>(height);
    depth_texture_.mipmaps = 1;
    depth_texture_.format = GL_DEPTH_COMPONENT32F;
  }

  auto begin() const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
  }

  auto begin_tile(ecs::ShadowAtlasTile const& tile) const -> void {
    glViewport(tile.x, tile.y, tile.width, tile.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(tile.x, tile.y, tile.width, tile.height);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
  }

  auto end() const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  auto bind(i32 slot) const -> void {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, depth_texture_.id);
  }

  auto unload() -> void {
    if (depth_texture_.id != 0) {
      glDeleteTextures(1, &depth_texture_.id);
      depth_texture_ = Texture{};
    }
    if (framebuffer_ != 0) {
      glDeleteFramebuffers(1, &framebuffer_);
      framebuffer_ = 0;
    }
    width_ = 0;
    height_ = 0;
  }

private:
  u32 framebuffer_ = 0;
  Texture depth_texture_{};
  u32 width_ = 0;
  u32 height_ = 0;
};

class DepthTarget2D {
public:
  DepthTarget2D() = default;
  DepthTarget2D(DepthTarget2D const&) = delete;
  auto operator=(DepthTarget2D const&) -> DepthTarget2D& = delete;

  DepthTarget2D(DepthTarget2D&& other) noexcept
      : framebuffer_(std::exchange(other.framebuffer_, 0)),
        depth_texture_(std::exchange(other.depth_texture_, {})),
        width_(std::exchange(other.width_, 0)), height_(std::exchange(other.height_, 0)) {}

  auto operator=(DepthTarget2D&& other) noexcept -> DepthTarget2D& {
    if (this == &other) {
      return *this;
    }
    unload();
    framebuffer_ = std::exchange(other.framebuffer_, 0);
    depth_texture_ = std::exchange(other.depth_texture_, {});
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    return *this;
  }

  ~DepthTarget2D() { unload(); }

  [[nodiscard]] auto ready() const -> bool {
    return framebuffer_ != 0 && depth_texture_.id != 0;
  }

  [[nodiscard]] auto texture() const -> Texture const& { return depth_texture_; }

  auto ensure(u32 width, u32 height) -> void {
    if (ready() && width_ == width && height_ == height) {
      return;
    }

    unload();

    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

    glGenTextures(1, &depth_texture_.id);
    glBindTexture(GL_TEXTURE_2D, depth_texture_.id);
    glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_DEPTH_COMPONENT32F,
      static_cast<GLsizei>(width),
      static_cast<GLsizei>(height),
      0,
      GL_DEPTH_COMPONENT,
      GL_FLOAT,
      nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float const border_color[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

    glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_.id, 0
    );
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    depth_texture_.width = static_cast<i32>(width);
    depth_texture_.height = static_cast<i32>(height);
    depth_texture_.mipmaps = 1;
    depth_texture_.format = GL_DEPTH_COMPONENT32F;
  }

  auto begin() const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    glClear(GL_DEPTH_BUFFER_BIT);
  }

  auto end() const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  auto bind(i32 slot) const -> void {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, depth_texture_.id);
  }

  auto unload() -> void {
    if (depth_texture_.id != 0) {
      glDeleteTextures(1, &depth_texture_.id);
      depth_texture_ = Texture{};
    }
    if (framebuffer_ != 0) {
      glDeleteFramebuffers(1, &framebuffer_);
      framebuffer_ = 0;
    }
    width_ = 0;
    height_ = 0;
  }

private:
  u32 framebuffer_ = 0;
  Texture depth_texture_{};
  u32 width_ = 0;
  u32 height_ = 0;
};

class DepthCubemapTarget {
public:
  DepthCubemapTarget() = default;
  DepthCubemapTarget(DepthCubemapTarget const&) = delete;
  auto operator=(DepthCubemapTarget const&) -> DepthCubemapTarget& = delete;

  DepthCubemapTarget(DepthCubemapTarget&& other) noexcept
      : framebuffer_(std::exchange(other.framebuffer_, 0)),
        cubemap_id_(std::exchange(other.cubemap_id_, 0)),
        width_(std::exchange(other.width_, 0)), height_(std::exchange(other.height_, 0)) {}

  auto operator=(DepthCubemapTarget&& other) noexcept -> DepthCubemapTarget& {
    if (this == &other) {
      return *this;
    }
    unload();
    framebuffer_ = std::exchange(other.framebuffer_, 0);
    cubemap_id_ = std::exchange(other.cubemap_id_, 0);
    width_ = std::exchange(other.width_, 0);
    height_ = std::exchange(other.height_, 0);
    return *this;
  }

  ~DepthCubemapTarget() { unload(); }

  [[nodiscard]] auto ready() const -> bool {
    return framebuffer_ != 0 && cubemap_id_ != 0;
  }

  [[nodiscard]] auto texture_id() const -> u32 { return cubemap_id_; }

  auto ensure(u32 width, u32 height) -> void {
    if (ready() && width_ == width && height_ == height) {
      return;
    }

    unload();
    width_ = width;
    height_ = height;

    glGenFramebuffers(1, &framebuffer_);
    glGenTextures(1, &cubemap_id_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_id_);
    for (u32 face = 0; face < 6; ++face) {
      glTexImage2D(
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
        0,
        GL_DEPTH_COMPONENT32F,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr
      );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, cubemap_id_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  auto begin_face(i32 face_index) const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(
      GL_FRAMEBUFFER,
      GL_DEPTH_ATTACHMENT,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_index,
      cubemap_id_,
      0
    );
    glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    glClear(GL_DEPTH_BUFFER_BIT);
  }

  auto end() const -> void {
    rl::rlDrawRenderBatchActive();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  auto bind(i32 slot) const -> void {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap_id_);
  }

  auto unload() -> void {
    if (cubemap_id_ != 0) {
      glDeleteTextures(1, &cubemap_id_);
      cubemap_id_ = 0;
    }
    if (framebuffer_ != 0) {
      glDeleteFramebuffers(1, &framebuffer_);
      framebuffer_ = 0;
    }
    width_ = 0;
    height_ = 0;
  }

private:
  u32 framebuffer_ = 0;
  u32 cubemap_id_ = 0;
  u32 width_ = 0;
  u32 height_ = 0;
};

} // namespace gl

namespace ecs {

struct ShadowResources {
  using T = Resource;

  gl::DepthAtlas directional_atlas{};
  std::array<gl::DepthCubemapTarget, MAX_POINT_SHADOWS> point_maps{};

  auto ensure(ShadowSettings const& settings) -> void {
    u32 const atlas_extent = settings.cascade_resolution * 2U;
    directional_atlas.ensure(atlas_extent, atlas_extent);

    for (i32 point_index = 0; point_index < settings.max_shadowed_point_lights_clamped();
         ++point_index) {
      point_maps[point_index].ensure(
        settings.point_shadow_resolution, settings.point_shadow_resolution
      );
    }
  }

  [[nodiscard]] auto directional_tile(i32 cascade_index) const -> ShadowAtlasTile {
    i32 const tile_x = cascade_index % 2;
    i32 const tile_y = cascade_index / 2;
    i32 const tile_width = static_cast<i32>(directional_atlas.width() / 2U);
    i32 const tile_height = static_cast<i32>(directional_atlas.height() / 2U);
    return ShadowAtlasTile{
      .x = tile_x * tile_width,
      .y = tile_y * tile_height,
      .width = tile_width,
      .height = tile_height,
      .uv_rect = {
        static_cast<f32>(tile_x) * 0.5F,
        static_cast<f32>(tile_y) * 0.5F,
        0.5F,
        0.5F,
      },
    };
  }

  auto bind_shader(gl::ShaderProgram& shader) const -> void {
    directional_atlas.bind(SHADOW_ATLAS_TEXTURE_SLOT);
    shader.set_sampler("u_shadow_atlas", SHADOW_ATLAS_TEXTURE_SLOT);

    for (i32 point_index = 0; point_index < MAX_POINT_SHADOWS; ++point_index) {
      i32 const slot = POINT_SHADOW_TEXTURE_SLOT_BASE + point_index;
      point_maps[point_index].bind(slot);
      shader.set_sampler(
        std::string{"u_point_shadow_map"} + std::to_string(point_index), slot
      );
    }
  }
};

} // namespace ecs
