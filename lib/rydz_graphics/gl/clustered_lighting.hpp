#pragma once

#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "types.hpp"
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace gl {

struct ClusterConfig {
  using T = ecs::Resource;

  i32 tile_count_x = 16;
  i32 tile_count_y = 9;
  i32 slice_count_z = 24;
  u32 max_point_lights = 1024;
  u32 max_lights_per_cluster = 128;

  [[nodiscard]] u32 cluster_count() const {
    return static_cast<u32>(tile_count_x * tile_count_y * slice_count_z);
  }

  [[nodiscard]] u32 max_light_indices() const {
    return cluster_count() * max_lights_per_cluster;
  }
};

struct alignas(16) GpuPointLight {
  Vec4 position_range = {0, 0, 0, 0};
  Vec4 color_intensity = {0, 0, 0, 0};
};

struct alignas(16) ClusterGpuRecord {
  Vec4 min_bounds = {0, 0, 0, 0};
  Vec4 max_bounds = {0, 0, 0, 0};
  std::array<u32, 4> meta{};
};

static constexpr auto POINT_LIGHT_SIZE = 32;
static constexpr auto CLUSTER_RECORD_SIZE = 48;

static_assert(sizeof(GpuPointLight) == POINT_LIGHT_SIZE);
static_assert(sizeof(ClusterGpuRecord) == CLUSTER_RECORD_SIZE);

struct ClusteredLightingState {
  using T = ecs::Resource;

  u32 point_light_buffer = 0;
  u32 cluster_buffer = 0;
  u32 light_index_buffer = 0;
  u32 overflow_buffer = 0;

  std::vector<GpuPointLight> point_lights_cpu;
  std::vector<ClusterGpuRecord> clusters_cpu;
  std::vector<u32> light_indices_cpu;

public:
  ClusteredLightingState() = default;
  ClusteredLightingState(const ClusteredLightingState &) = delete;
  ClusteredLightingState &operator=(const ClusteredLightingState &) = delete;
  ClusteredLightingState(ClusteredLightingState &&other) noexcept
      : point_light_buffer(other.point_light_buffer),
        cluster_buffer(other.cluster_buffer),
        light_index_buffer(other.light_index_buffer),
        overflow_buffer(other.overflow_buffer),
        point_lights_cpu(std::move(other.point_lights_cpu)),
        clusters_cpu(std::move(other.clusters_cpu)),
        light_indices_cpu(std::move(other.light_indices_cpu)) {
    other.point_light_buffer = 0;
    other.cluster_buffer = 0;
    other.light_index_buffer = 0;
    other.overflow_buffer = 0;
  }

  ClusteredLightingState &operator=(ClusteredLightingState &&other) noexcept {
    if (this == &other) {
      return *this;
    }

    release_buffers();
    point_light_buffer = other.point_light_buffer;
    cluster_buffer = other.cluster_buffer;
    light_index_buffer = other.light_index_buffer;
    overflow_buffer = other.overflow_buffer;
    point_lights_cpu = std::move(other.point_lights_cpu);
    clusters_cpu = std::move(other.clusters_cpu);
    light_indices_cpu = std::move(other.light_indices_cpu);
    other.point_light_buffer = 0;
    other.cluster_buffer = 0;
    other.light_index_buffer = 0;
    other.overflow_buffer = 0;
    return *this;
  }

  ~ClusteredLightingState() { release_buffers(); }

  void release_buffers() {
    if (point_light_buffer != 0) {
      rl::rlUnloadShaderBuffer(point_light_buffer);
      point_light_buffer = 0;
    }
    if (cluster_buffer != 0) {
      rl::rlUnloadShaderBuffer(cluster_buffer);
      cluster_buffer = 0;
    }
    if (light_index_buffer != 0) {
      rl::rlUnloadShaderBuffer(light_index_buffer);
      light_index_buffer = 0;
    }
    if (overflow_buffer != 0) {
      rl::rlUnloadShaderBuffer(overflow_buffer);
      overflow_buffer = 0;
    }
  }

  void ensure_buffers(const ClusterConfig &config) {
    if (point_light_buffer == 0) {
      point_light_buffer = rl::rlLoadShaderBuffer(sizeof(GpuPointLight) *
                                                      config.max_point_lights,
                                                  nullptr, RL_DYNAMIC_DRAW);
    }
    if (cluster_buffer == 0) {
      cluster_buffer = rl::rlLoadShaderBuffer(sizeof(ClusterGpuRecord) *
                                                  config.cluster_count(),
                                              nullptr, RL_DYNAMIC_DRAW);
    }
    if (light_index_buffer == 0) {
      light_index_buffer = rl::rlLoadShaderBuffer(
          sizeof(u32) * config.max_light_indices(), nullptr, RL_DYNAMIC_DRAW);
    }
    if (overflow_buffer == 0) {
      overflow_buffer =
          rl::rlLoadShaderBuffer(sizeof(u32), nullptr, RL_DYNAMIC_DRAW);
    }
  }

  void reset_cpu_storage(const ClusterConfig &config) {
    point_lights_cpu.clear();
    point_lights_cpu.reserve(config.max_point_lights);
    clusters_cpu.clear();
    clusters_cpu.resize(config.cluster_count());
    light_indices_cpu.clear();
    light_indices_cpu.resize(config.max_light_indices(), 0);
  }
};

inline void bind_clustered_lighting(const ClusteredLightingState &state) {
  if (state.point_light_buffer != 0) {
    rl::rlBindShaderBuffer(state.point_light_buffer, 0);
  }
  if (state.cluster_buffer != 0) {
    rl::rlBindShaderBuffer(state.cluster_buffer, 1);
  }
  if (state.light_index_buffer != 0) {
    rl::rlBindShaderBuffer(state.light_index_buffer, 2);
  }
  if (state.overflow_buffer != 0) {
    rl::rlBindShaderBuffer(state.overflow_buffer, 3);
  }
}

inline void update_shader_buffer(u32 buffer, const void *data,
                                 unsigned int size, unsigned int offset = 0) {
  rl::rlUpdateShaderBuffer(buffer, data, size, offset);
}

inline f32 cluster_slice_distance(const ClusterConfig &config, f32 near_plane,
                                  f32 far_plane, bool orthographic,
                                  i32 slice_index) {
  constexpr auto MARGIN = 0.001F;
  f32 clamped_near = std::max(near_plane, MARGIN);
  f32 clamped_far = std::max(far_plane, clamped_near + MARGIN);
  f32 alpha =
      static_cast<f32>(slice_index) / static_cast<f32>(config.slice_count_z);

  if (orthographic) {
    return clamped_near + ((clamped_far - clamped_near) * alpha);
  }

  f32 ratio = clamped_far / clamped_near;
  return clamped_near * std::pow(ratio, alpha);
}

inline math::Vec3 unproject_to_view(const math::Mat4 &inverse_projection,
                                    f32 ndc_x, f32 ndc_y, f32 ndc_z) {
  math::Vec4 clip(ndc_x, ndc_y, ndc_z, 1.0F);
  math::Vec4 view = inverse_projection * clip;
  f32 w = view.GetW();
  f32 inv_w = std::abs(w) > 1e-6F ? 1.0F / w : 1.0F;
  return {view.GetX() * inv_w, view.GetY() * inv_w, view.GetZ() * inv_w};
}

inline ClusterGpuRecord
build_cluster_record(const ClusterConfig &config,
                     const math::Mat4 &inverse_projection, bool orthographic,
                     f32 near_plane, f32 far_plane, i32 tile_x, i32 tile_y,
                     i32 tile_z) {
  ClusterGpuRecord record{};

  const u32 cluster_index = static_cast<u32>(
      ((tile_z * config.tile_count_y + tile_y) * config.tile_count_x) + tile_x);
  record.meta[0] = cluster_index * config.max_lights_per_cluster;
  record.meta[1] = 0;
  record.meta[2] = 0;
  record.meta[3] = 0;

  const f32 ndc_x0 =
      -1.0F + (2.0F * static_cast<f32>(tile_x) / config.tile_count_x);
  const f32 ndc_x1 =
      -1.0F + (2.0F * static_cast<f32>(tile_x + 1) / config.tile_count_x);
  const f32 ndc_y0 =
      -1.0F + (2.0F * static_cast<f32>(tile_y) / config.tile_count_y);
  const f32 ndc_y1 =
      -1.0F + (2.0F * static_cast<f32>(tile_y + 1) / config.tile_count_y);

  const f32 slice_near = cluster_slice_distance(config, near_plane, far_plane,
                                                orthographic, tile_z);
  const f32 slice_far = cluster_slice_distance(config, near_plane, far_plane,
                                               orthographic, tile_z + 1);

  math::AABox bbox;
  bbox.mMin = math::Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  bbox.mMax = math::Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  const std::array<std::pair<f32, f32>, 4> corners = {
      std::pair{ndc_x0, ndc_y0}, std::pair{ndc_x1, ndc_y0},
      std::pair{ndc_x0, ndc_y1}, std::pair{ndc_x1, ndc_y1}};

  if (orthographic) {
    const f32 depth_range = std::max(far_plane - near_plane, 0.001F);
    const f32 t_near = (slice_near - near_plane) / depth_range;
    const f32 t_far = (slice_far - near_plane) / depth_range;

    for (const auto &[corner_x, corner_y] : corners) {
      const math::Vec3 near_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, -1.0F);
      const math::Vec3 far_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, 1.0F);
      const math::Vec3 direction = far_corner - near_corner;
      bbox.Encapsulate(near_corner + direction * t_near);
      bbox.Encapsulate(near_corner + direction * t_far);
    }
  } else {
    const f32 camera_near = std::max(near_plane, 0.001F);

    for (const auto &[corner_x, corner_y] : corners) {
      const math::Vec3 near_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, -1.0F);
      bbox.Encapsulate(near_corner * (slice_near / camera_near));
      bbox.Encapsulate(near_corner * (slice_far / camera_near));
    }
  }

  record.min_bounds = {.x = bbox.mMin.GetX(),
                       .y = bbox.mMin.GetY(),
                       .z = bbox.mMin.GetZ(),
                       .w = 0.0F};
  record.max_bounds = {.x = bbox.mMax.GetX(),
                       .y = bbox.mMax.GetY(),
                       .z = bbox.mMax.GetZ(),
                       .w = 0.0F};
  return record;
}

} // namespace gl
