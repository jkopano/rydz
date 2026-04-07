#pragma once

#include "math.hpp"
#include "rl.hpp"
#include "rydz_camera/camera3d.hpp"
#include "rydz_ecs/fwd.hpp"
#include "types.hpp"
#include <array>
#include <cfloat>
#include <cmath>
#include <utility>
#include <vector>

namespace ecs {

using namespace math;

struct ClusterConfig {
  using T = Resource;

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
  rl::Vector4 position_range = {0, 0, 0, 0};
  rl::Vector4 color_intensity = {0, 0, 0, 0};
};

struct alignas(16) ClusterGpuRecord {
  rl::Vector4 min_bounds = {0, 0, 0, 0};
  rl::Vector4 max_bounds = {0, 0, 0, 0};
  std::array<u32, 4> meta{};
};

static_assert(sizeof(GpuPointLight) == 32);
static_assert(sizeof(ClusterGpuRecord) == 48);

struct ClusteredLightingState {
  using T = Resource;

  u32 point_light_buffer = 0;
  u32 cluster_buffer = 0;
  u32 light_index_buffer = 0;
  u32 overflow_buffer = 0;

  std::vector<GpuPointLight> point_lights_cpu;
  std::vector<ClusterGpuRecord> clusters_cpu;
  std::vector<u32> light_indices_cpu;

  ClusteredLightingState() = default;
  ClusteredLightingState(const ClusteredLightingState &) = delete;
  ClusteredLightingState &operator=(const ClusteredLightingState &) = delete;
  ClusteredLightingState(ClusteredLightingState &&other) noexcept
      : point_light_buffer(other.point_light_buffer),
        cluster_buffer(other.cluster_buffer),
        light_index_buffer(other.light_index_buffer),
        overflow_buffer(other.overflow_buffer),
        point_lights_cpu(std::move(other.point_lights_cpu)),
        clusters_cpu(std::move(other.clusters_cpu)) {
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

inline float cluster_slice_distance(const ClusterConfig &config,
                                    float near_plane, float far_plane,
                                    bool orthographic, i32 slice_index) {
  float clamped_near = std::max(near_plane, 0.001f);
  float clamped_far = std::max(far_plane, clamped_near + 0.001f);
  float alpha = static_cast<float>(slice_index) /
                static_cast<float>(config.slice_count_z);

  if (orthographic) {
    return clamped_near + (clamped_far - clamped_near) * alpha;
  }

  float ratio = clamped_far / clamped_near;
  return clamped_near * std::pow(ratio, alpha);
}

inline Vec3 unproject_to_view(const Mat4 &inverse_projection, float ndc_x,
                              float ndc_y, float ndc_z) {
  Vec4 clip(ndc_x, ndc_y, ndc_z, 1.0f);
  Vec4 view = inverse_projection * clip;
  float w = view.GetW();
  float inv_w = std::abs(w) > 1e-6f ? 1.0f / w : 1.0f;
  return Vec3(view.GetX() * inv_w, view.GetY() * inv_w, view.GetZ() * inv_w);
}

inline ClusterGpuRecord build_cluster_record(const ClusterConfig &config,
                                             const Mat4 &inverse_projection,
                                             bool orthographic,
                                             float near_plane, float far_plane,
                                             i32 tile_x, i32 tile_y,
                                             i32 tile_z) {
  ClusterGpuRecord record{};

  const u32 cluster_index = static_cast<u32>(
      (tile_z * config.tile_count_y + tile_y) * config.tile_count_x + tile_x);
  record.meta[0] = cluster_index * config.max_lights_per_cluster;
  record.meta[1] = 0;
  record.meta[2] = 0;
  record.meta[3] = 0;

  const float ndc_x0 =
      -1.0f + 2.0f * static_cast<float>(tile_x) / config.tile_count_x;
  const float ndc_x1 =
      -1.0f + 2.0f * static_cast<float>(tile_x + 1) / config.tile_count_x;
  const float ndc_y0 =
      -1.0f + 2.0f * static_cast<float>(tile_y) / config.tile_count_y;
  const float ndc_y1 =
      -1.0f + 2.0f * static_cast<float>(tile_y + 1) / config.tile_count_y;

  const float slice_near = cluster_slice_distance(config, near_plane, far_plane,
                                                  orthographic, tile_z);
  const float slice_far = cluster_slice_distance(config, near_plane, far_plane,
                                                 orthographic, tile_z + 1);

  AABox bbox;
  bbox.mMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  bbox.mMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  const std::array<std::pair<float, float>, 4> corners = {
      std::pair{ndc_x0, ndc_y0}, std::pair{ndc_x1, ndc_y0},
      std::pair{ndc_x0, ndc_y1}, std::pair{ndc_x1, ndc_y1}};

  if (orthographic) {
    const float depth_range = std::max(far_plane - near_plane, 0.001f);
    const float t_near = (slice_near - near_plane) / depth_range;
    const float t_far = (slice_far - near_plane) / depth_range;

    for (const auto &[corner_x, corner_y] : corners) {
      const Vec3 near_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, -1.0f);
      const Vec3 far_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, 1.0f);
      const Vec3 direction = far_corner - near_corner;
      bbox.Encapsulate(near_corner + direction * t_near);
      bbox.Encapsulate(near_corner + direction * t_far);
    }
  } else {
    const float camera_near = std::max(near_plane, 0.001f);

    for (const auto &[corner_x, corner_y] : corners) {
      const Vec3 near_corner =
          unproject_to_view(inverse_projection, corner_x, corner_y, -1.0f);
      bbox.Encapsulate(near_corner * (slice_near / camera_near));
      bbox.Encapsulate(near_corner * (slice_far / camera_near));
    }
  }

  record.min_bounds = {bbox.mMin.GetX(), bbox.mMin.GetY(), bbox.mMin.GetZ(),
                       0.0f};
  record.max_bounds = {bbox.mMax.GetX(), bbox.mMax.GetY(), bbox.mMax.GetZ(),
                       0.0f};
  return record;
}

} // namespace ecs
