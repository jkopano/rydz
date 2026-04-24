#pragma once

#include "math.hpp"
#include "rydz_ecs/fwd.hpp"
#include "rydz_graphics/extract/systems.hpp"
#include "rydz_graphics/gl/buffers.hpp"
#include "rydz_graphics/gl/compute.hpp"
#include "rydz_log/mod.hpp"
#include "types.hpp"
#include <algorithm>
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

  [[nodiscard]] auto tile_count_x_clamped() const -> i32 {
    return std::max(tile_count_x, 1);
  }

  [[nodiscard]] auto tile_count_y_clamped() const -> i32 {
    return std::max(tile_count_y, 1);
  }

  [[nodiscard]] auto slice_count_z_clamped() const -> i32 {
    return std::max(slice_count_z, 1);
  }

  [[nodiscard]] auto max_lights_per_cluster_clamped() const -> u32 {
    return std::max(max_lights_per_cluster, 1U);
  }

  [[nodiscard]] auto cluster_count() const -> u32 {
    return static_cast<u32>(
      tile_count_x_clamped() * tile_count_y_clamped() * slice_count_z_clamped()
    );
  }

  [[nodiscard]] auto max_light_indices() const -> u32 {
    return cluster_count() * max_lights_per_cluster_clamped();
  }
};

struct alignas(16) GpuPointLight {
  Vec4 position_range = {0, 0, 0, 0};
  Vec4 color_intensity = {0, 0, 0, 0};
  Vec4 shadow_data = {-1, 0, 0, 0};
};

struct alignas(16) ClusterGpuRecord {
  Vec4 min_bounds = {0, 0, 0, 0};
  Vec4 max_bounds = {0, 0, 0, 0};
  u32 light_index_offset = 0;
  u32 light_count = 0;
  u32 _pad0 = 0;
  u32 _pad1 = 0;
};

static constexpr auto POINT_LIGHT_SIZE = 48;
static constexpr auto CLUSTER_RECORD_SIZE = 48;

static_assert(sizeof(GpuPointLight) == POINT_LIGHT_SIZE);
static_assert(sizeof(ClusterGpuRecord) == CLUSTER_RECORD_SIZE);

struct ClusterConfigSnapshot {
  i32 tile_count_x = 0;
  i32 tile_count_y = 0;
  i32 slice_count_z = 0;
  u32 max_point_lights = 0;
  u32 max_lights_per_cluster = 0;

  auto operator==(ClusterConfigSnapshot const&) const -> bool = default;

  static auto from(ClusterConfig const& config) -> ClusterConfigSnapshot {
    return {
      .tile_count_x = config.tile_count_x_clamped(),
      .tile_count_y = config.tile_count_y_clamped(),
      .slice_count_z = config.slice_count_z_clamped(),
      .max_point_lights = config.max_point_lights,
      .max_lights_per_cluster = config.max_lights_per_cluster_clamped(),
    };
  }
};

inline auto cluster_slice_distance(
  ClusterConfig const& config,
  f32 near_plane,
  f32 far_plane,
  bool orthographic,
  i32 slice_index
) -> f32 {
  constexpr auto MARGIN = 0.001F;
  f32 clamped_near = std::max(near_plane, MARGIN);
  f32 clamped_far = std::max(far_plane, clamped_near + MARGIN);
  f32 alpha =
    static_cast<f32>(slice_index) / static_cast<f32>(config.slice_count_z_clamped());

  if (orthographic) {
    return clamped_near + ((clamped_far - clamped_near) * alpha);
  }

  f32 ratio = clamped_far / clamped_near;
  return clamped_near * std::pow(ratio, alpha);
}

inline auto unproject_to_view(
  math::Mat4 const& inverse_projection, f32 ndc_x, f32 ndc_y, f32 ndc_z
) -> math::Vec3 {
  math::Vec4 clip(ndc_x, ndc_y, ndc_z, 1.0F);
  math::Vec4 view = inverse_projection * clip;
  f32 w = view.w;
  f32 inv_w = std::abs(w) > 1e-6F ? 1.0F / w : 1.0F;
  return {view.x * inv_w, view.y * inv_w, view.z * inv_w};
}

inline auto build_cluster_record(
  ClusterConfig const& config,
  math::Mat4 const& inverse_projection,
  bool orthographic,
  f32 near_plane,
  f32 far_plane,
  i32 tile_x,
  i32 tile_y,
  i32 tile_z
) -> ClusterGpuRecord {
  ClusterGpuRecord record{};
  i32 const tile_count_x = config.tile_count_x_clamped();
  i32 const tile_count_y = config.tile_count_y_clamped();
  u32 const max_lights_per_cluster = config.max_lights_per_cluster_clamped();

  u32 const cluster_index =
    static_cast<u32>(((tile_z * tile_count_y + tile_y) * tile_count_x) + tile_x);
  record.light_index_offset = cluster_index * max_lights_per_cluster;
  record.light_count = 0;
  record._pad0 = 0;
  record._pad1 = 0;

  f32 const ndc_x0 = -1.0F + (2.0F * static_cast<f32>(tile_x) / tile_count_x);
  f32 const ndc_x1 = -1.0F + (2.0F * static_cast<f32>(tile_x + 1) / tile_count_x);
  f32 const ndc_y0 = -1.0F + (2.0F * static_cast<f32>(tile_y) / tile_count_y);
  f32 const ndc_y1 = -1.0F + (2.0F * static_cast<f32>(tile_y + 1) / tile_count_y);

  f32 const slice_near =
    cluster_slice_distance(config, near_plane, far_plane, orthographic, tile_z);
  f32 const slice_far =
    cluster_slice_distance(config, near_plane, far_plane, orthographic, tile_z + 1);

  math::AABox bbox;
  bbox.mMin = math::Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
  bbox.mMax = math::Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  std::array<std::pair<f32, f32>, 4> const corners = {
    std::pair{ndc_x0, ndc_y0},
    std::pair{ndc_x1, ndc_y0},
    std::pair{ndc_x0, ndc_y1},
    std::pair{ndc_x1, ndc_y1}
  };

  if (orthographic) {
    f32 const depth_range = std::max(far_plane - near_plane, 0.001F);
    f32 const t_near = (slice_near - near_plane) / depth_range;
    f32 const t_far = (slice_far - near_plane) / depth_range;

    for (auto const& [corner_x, corner_y] : corners) {
      math::Vec3 const near_corner =
        unproject_to_view(inverse_projection, corner_x, corner_y, -1.0F);
      math::Vec3 const far_corner =
        unproject_to_view(inverse_projection, corner_x, corner_y, 1.0F);
      math::Vec3 const direction = far_corner - near_corner;
      bbox.encapsulate(near_corner + direction * t_near);
      bbox.encapsulate(near_corner + direction * t_far);
    }
  } else {
    f32 const camera_near = std::max(near_plane, 0.001F);

    for (auto const& [corner_x, corner_y] : corners) {
      math::Vec3 const near_corner =
        unproject_to_view(inverse_projection, corner_x, corner_y, -1.0F);
      bbox.encapsulate(near_corner * (slice_near / camera_near));
      bbox.encapsulate(near_corner * (slice_far / camera_near));
    }
  }

  record.min_bounds = Vec4{bbox.mMin.x, bbox.mMin.y, bbox.mMin.z, 0.0F};
  record.max_bounds = Vec4{bbox.mMax.x, bbox.mMax.y, bbox.mMax.z, 0.0F};
  return record;
}

struct ClusteredLightingState {
  using T = ecs::Resource;

  SSBO point_light_buffer{};
  SSBO cluster_buffer{};
  SSBO light_index_buffer{};
  SSBO overflow_buffer{};
  ComputeProgram cluster_build_program{};

  std::vector<GpuPointLight> point_lights_cpu;
  std::vector<ClusterGpuRecord> clusters_cpu;

  math::Mat4 last_proj = math::Mat4::IDENTITY;
  ClusterConfigSnapshot last_config{};
  f32 last_near = -1.0F;
  f32 last_far = -1.0F;
  bool last_ortho = false;
  bool last_config_valid = false;
  bool clusters_dirty = true;
  u32 point_light_buffer_bytes = 0;
  u32 cluster_buffer_bytes = 0;
  u32 light_index_buffer_bytes = 0;
  bool cluster_build_program_failed = false;

public:
  ClusteredLightingState() = default;
  ClusteredLightingState(ClusteredLightingState const&) = delete;
  auto operator=(ClusteredLightingState const&) -> ClusteredLightingState& = delete;
  ClusteredLightingState(ClusteredLightingState&&) noexcept = default;
  auto operator=(ClusteredLightingState&&) noexcept -> ClusteredLightingState& = default;

  auto ensure_buffers(ClusterConfig const& config) -> void {
    u32 const point_light_bytes =
      static_cast<u32>(sizeof(GpuPointLight) * std::max(config.max_point_lights, 1U));
    u32 const cluster_bytes =
      static_cast<u32>(sizeof(ClusterGpuRecord) * config.cluster_count());
    u32 const light_index_bytes =
      static_cast<u32>(sizeof(u32) * config.max_light_indices());

    if (!point_light_buffer.ready() || point_light_buffer_bytes != point_light_bytes) {
      point_light_buffer = SSBO(point_light_bytes, nullptr, RL_DYNAMIC_DRAW);
      point_light_buffer_bytes = point_light_bytes;
    }
    if (!cluster_buffer.ready() || cluster_buffer_bytes != cluster_bytes) {
      cluster_buffer = SSBO(cluster_bytes, nullptr, RL_DYNAMIC_DRAW);
      cluster_buffer_bytes = cluster_bytes;
    }
    if (!light_index_buffer.ready() || light_index_buffer_bytes != light_index_bytes) {
      light_index_buffer = SSBO(light_index_bytes, nullptr, RL_DYNAMIC_DRAW);
      light_index_buffer_bytes = light_index_bytes;
    }
    if (!overflow_buffer.ready()) {
      overflow_buffer = SSBO(sizeof(u32), nullptr, RL_DYNAMIC_DRAW);
    }
  }

  [[nodiscard]] auto ensure_compute_program() -> bool {
    if (cluster_build_program.ready()) {
      return true;
    }
    if (cluster_build_program_failed) {
      return false;
    }

    cluster_build_program_failed =
      !cluster_build_program.load("res/shaders/cluster_build.comp");
    return !cluster_build_program_failed;
  }

  auto build_cluster_buffers(
    ecs::NonSendMarker marker,
    ecs::ExtractedView const& view,
    ecs::ExtractedLights const& lights,
    ClusterConfig const& config
  ) -> void {
    (void) marker;

    ensure_buffers(config);

    ClusterConfigSnapshot const config_snapshot = ClusterConfigSnapshot::from(config);
    if (!last_config_valid || last_config != config_snapshot) {
      clusters_dirty = true;
      last_config = config_snapshot;
      last_config_valid = true;
    }

    if (clusters_cpu.size() != config.cluster_count()) {
      clusters_dirty = true;
    }

    if (last_proj != view.camera_view.proj || last_near != view.near_plane ||
        last_far != view.far_plane || last_ortho != view.orthographic) {
      clusters_dirty = true;
      last_proj = view.camera_view.proj;
      last_near = view.near_plane;
      last_far = view.far_plane;
      last_ortho = view.orthographic;
    }

    point_lights_cpu.clear();
    point_lights_cpu.reserve(config.max_point_lights);

    if (clusters_dirty) {
      clusters_cpu.clear();
      clusters_cpu.resize(config.cluster_count());
    }

    usize const point_light_count =
      std::min(lights.point_lights.size(), static_cast<usize>(config.max_point_lights));
    if (lights.point_lights.size() > point_light_count) {
      warn(
        "Forward+: dropping {} point lights beyonds configured cap",
        static_cast<int>(lights.point_lights.size() - point_light_count)
      );
    }

    for (usize i = 0; i < point_light_count; ++i) {
      auto const& light = lights.point_lights[i];
      point_lights_cpu.push_back(
        GpuPointLight{
          .position_range =
            {light.position.x, light.position.y, light.position.z, light.range},
          .color_intensity = {
            light.color.r / 255.0f,
            light.color.g / 255.0f,
            light.color.b / 255.0f,
            light.intensity
          },
          .shadow_data = {
            static_cast<f32>(light.shadow_slot),
            light.range,
            0.0F,
            0.0F,
          },
        }
      );
    }

    if (clusters_dirty) {
      math::Mat4 const inverse_projection = view.camera_view.proj.inverse();
      i32 const tile_count_x = config.tile_count_x_clamped();
      i32 const tile_count_y = config.tile_count_y_clamped();
      i32 const slice_count_z = config.slice_count_z_clamped();

      for (i32 z = 0; z < slice_count_z; ++z) {
        for (i32 y = 0; y < tile_count_y; ++y) {
          for (i32 x = 0; x < tile_count_x; ++x) {
            u32 const cluster_index =
              static_cast<u32>(((z * tile_count_y + y) * tile_count_x) + x);
            clusters_cpu[cluster_index] = gl::build_cluster_record(
              config,
              inverse_projection,
              view.orthographic,
              view.near_plane,
              view.far_plane,
              x,
              y,
              z
            );
          }
        }
      }
      clusters_dirty = false;
    } else {
      for (auto& cluster : clusters_cpu) {
        cluster.light_count = 0;
        cluster._pad0 = 0;
        cluster._pad1 = 0;
      }
    }

    u32 overflow_count = 0;

    if (!point_lights_cpu.empty()) {
      point_light_buffer.update(
        point_lights_cpu.data(),
        static_cast<u32>(point_lights_cpu.size() * sizeof(GpuPointLight)),
        0
      );
    }

    cluster_buffer.update(
      clusters_cpu.data(),
      static_cast<u32>(clusters_cpu.size() * sizeof(ClusterGpuRecord)),
      0
    );

    overflow_buffer.update(&overflow_count, sizeof(overflow_count), 0);

    if (!point_lights_cpu.empty() && ensure_compute_program()) {
      dispatch_cluster_build(view, config);
    }
  }

  auto bind() const -> void {
    if (point_light_buffer.ready()) {
      point_light_buffer.bind(0);
    }
    if (cluster_buffer.ready()) {
      cluster_buffer.bind(1);
    }
    if (light_index_buffer.ready()) {
      light_index_buffer.bind(2);
    }
    if (overflow_buffer.ready()) {
      overflow_buffer.bind(3);
    }
  }

private:
  auto dispatch_cluster_build(ecs::ExtractedView const& view, ClusterConfig const& config)
    -> void {
    u32 const point_light_count = static_cast<u32>(point_lights_cpu.size());
    u32 const workgroups = (point_light_count + 63U) / 64U;
    if (workgroups == 0) {
      return;
    }

    point_light_buffer.bind(0);
    cluster_buffer.bind(1);
    light_index_buffer.bind(2);
    overflow_buffer.bind(3);

    u32 const program_id = cluster_build_program.id();
    rl::rlEnableShader(program_id);

    auto set_uint = [program_id](char const* name, u32 value) -> void {
      i32 const location = rl::rlGetLocationUniform(program_id, name);
      if (location >= 0) {
        rl::rlSetUniform(location, &value, SHADER_UNIFORM_UINT, 1);
      }
    };

    set_uint("u_point_light_count", point_light_count);
    set_uint("u_cluster_count", config.cluster_count());
    set_uint("u_max_lights_per_cluster", config.max_lights_per_cluster_clamped());

    i32 const view_location = rl::rlGetLocationUniform(program_id, "u_view");
    if (view_location >= 0) {
      Matrix const view_matrix = math::to_rl(view.camera_view.view);
      rl::rlSetUniformMatrix(view_location, view_matrix);
    }

    rl::rlComputeShaderDispatch(workgroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    rl::rlDisableShader();
  }
};

} // namespace gl

namespace ecs {

using gl::ClusterConfig;
using gl::ClusteredLightingState;
using gl::ClusterGpuRecord;
using gl::GpuPointLight;

} // namespace ecs
