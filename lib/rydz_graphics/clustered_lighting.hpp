#pragma once

#include "math.hpp"
#include "render_extract.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/gl/core.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <array>
#include <cfloat>
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

struct ClusteredLightingState {
  using T = ecs::Resource;

  SSBO point_light_buffer{};
  SSBO cluster_buffer{};
  SSBO light_index_buffer{};
  SSBO overflow_buffer{};

  std::vector<GpuPointLight> point_lights_cpu;
  std::vector<ClusterGpuRecord> clusters_cpu;
  std::vector<u32> light_indices_cpu;

public:
  ClusteredLightingState() = default;
  ClusteredLightingState(const ClusteredLightingState &) = delete;
  ClusteredLightingState &operator=(const ClusteredLightingState &) = delete;
  ClusteredLightingState(ClusteredLightingState &&) noexcept = default;
  ClusteredLightingState &
  operator=(ClusteredLightingState &&) noexcept = default;

  void ensure_buffers(const ClusterConfig &config) {
    if (!point_light_buffer.ready()) {
      point_light_buffer = SSBO(sizeof(GpuPointLight) * config.max_point_lights,
                                nullptr, RL_DYNAMIC_DRAW);
    }
    if (!cluster_buffer.ready()) {
      cluster_buffer = SSBO(sizeof(ClusterGpuRecord) * config.cluster_count(),
                            nullptr, RL_DYNAMIC_DRAW);
    }
    if (!light_index_buffer.ready()) {
      light_index_buffer = SSBO(sizeof(u32) * config.max_light_indices(),
                                nullptr, RL_DYNAMIC_DRAW);
    }
    if (!overflow_buffer.ready()) {
      overflow_buffer = SSBO(sizeof(u32), nullptr, RL_DYNAMIC_DRAW);
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

  void build_cluster_buffers(ecs::NonSendMarker marker,
                             const ecs::ExtractedView &view,
                             const ecs::ExtractedLights &lights,
                             const ClusterConfig &config) {
    static u32 last_reported_overflow = U32_MAX;

    ensure_buffers(config);
    reset_cpu_storage(config);

    const usize point_light_count =
        std::min(lights.point_lights.size(),
                 static_cast<usize>(config.max_point_lights));
    if (lights.point_lights.size() > point_light_count) {
      warn("Forward+: dropping {} point lights beyonds configured cap",
           static_cast<int>(lights.point_lights.size() - point_light_count));
    }

    for (usize i = 0; i < point_light_count; ++i) {
      const auto &light = lights.point_lights[i];
      point_lights_cpu.push_back(GpuPointLight{
          .position_range = {light.position.GetX(), light.position.GetY(),
                             light.position.GetZ(), light.range},
          .color_intensity = {light.color.r / 255.0f, light.color.g / 255.0f,
                              light.color.b / 255.0f, light.intensity},
      });
    }

    const math::Mat4 inverse_projection = view.camera_view.proj.Inversed();
    for (i32 z = 0; z < config.slice_count_z; ++z) {
      for (i32 y = 0; y < config.tile_count_y; ++y) {
        for (i32 x = 0; x < config.tile_count_x; ++x) {
          const u32 cluster_index = static_cast<u32>(
              ((z * config.tile_count_y + y) * config.tile_count_x) + x);
          clusters_cpu[cluster_index] = gl::build_cluster_record(
              config, inverse_projection, view.orthographic, view.near_plane,
              view.far_plane, x, y, z);
        }
      }
    }

    u32 overflow_count = 0;

    for (u32 light_index = 0;
         light_index < static_cast<u32>(point_lights_cpu.size());
         ++light_index) {
      const auto &light = point_lights_cpu[light_index];
      const math::Vec3 view_position_math =
          view.camera_view.view * math::Vec3(light.position_range.x,
                                             light.position_range.y,
                                             light.position_range.z);
      const auto view_position = math::to_rl(view_position_math);

      for (auto &cluster : clusters_cpu) {
        const f32 closest_x = std::clamp(view_position.x, cluster.min_bounds.x,
                                         cluster.max_bounds.x);
        const f32 closest_y = std::clamp(view_position.y, cluster.min_bounds.y,
                                         cluster.max_bounds.y);
        const f32 closest_z = std::clamp(view_position.z, cluster.min_bounds.z,
                                         cluster.max_bounds.z);
        const f32 dx = view_position.x - closest_x;
        const f32 dy = view_position.y - closest_y;
        const f32 dz = view_position.z - closest_z;
        if ((dx * dx) + (dy * dy) + dz * dz >
            light.position_range.w * light.position_range.w) {
          continue;
        }

        if (cluster.meta[1] >= config.max_lights_per_cluster) {
          ++overflow_count;
          continue;
        }

        const u32 offset = cluster.meta[0] + cluster.meta[1];
        light_indices_cpu[offset] = light_index;
        ++cluster.meta[1];
      }
    }

    if (!point_lights_cpu.empty()) {
      point_light_buffer.update(
          point_lights_cpu.data(),
          static_cast<u32>(point_lights_cpu.size() * sizeof(GpuPointLight)), 0);
    }

    cluster_buffer.update(
        clusters_cpu.data(),
        static_cast<u32>(clusters_cpu.size() * sizeof(ClusterGpuRecord)), 0);

    light_index_buffer.update(
        light_indices_cpu.data(),
        static_cast<u32>(light_indices_cpu.size() * sizeof(u32)), 0);

    overflow_buffer.update(&overflow_count, sizeof(overflow_count), 0);

    if (overflow_count > 0) {
      if (last_reported_overflow != overflow_count) {
        warn("Forward+: cluster light list overflowed {} writes",
             overflow_count);
        last_reported_overflow = overflow_count;
      }
    } else {
      last_reported_overflow = U32_MAX;
    }
  }
};

inline void bind_clustered_lighting(const ClusteredLightingState &state) {
  if (state.point_light_buffer.ready()) {
    state.point_light_buffer.bind(0);
  }
  if (state.cluster_buffer.ready()) {
    state.cluster_buffer.bind(1);
  }
  if (state.light_index_buffer.ready()) {
    state.light_index_buffer.bind(2);
  }
  if (state.overflow_buffer.ready()) {
    state.overflow_buffer.bind(3);
  }
}

} // namespace gl

namespace ecs {

using gl::ClusterConfig;
using gl::ClusteredLightingState;
using gl::ClusterGpuRecord;
using gl::GpuPointLight;

} // namespace ecs
