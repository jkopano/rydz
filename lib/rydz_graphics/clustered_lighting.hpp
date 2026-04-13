#pragma once

#include "math.hpp"
#include "rydz_graphics/gl/clustered_lighting.hpp"

namespace ecs {

using namespace math;

using ClusterConfig = gl::ClusterConfig;
using GpuPointLight = gl::GpuPointLight;
using ClusterGpuRecord = gl::ClusterGpuRecord;
using ClusteredLightingState = gl::ClusteredLightingState;

inline float cluster_slice_distance(const ClusterConfig &config,
                                    float near_plane, float far_plane,
                                    bool orthographic, i32 slice_index) {
  return gl::cluster_slice_distance(config, near_plane, far_plane,
                                         orthographic, slice_index);
}

inline Vec3 unproject_to_view(const Mat4 &inverse_projection, float ndc_x,
                              float ndc_y, float ndc_z) {
  return gl::unproject_to_view(inverse_projection, ndc_x, ndc_y, ndc_z);
}

inline ClusterGpuRecord build_cluster_record(const ClusterConfig &config,
                                             const Mat4 &inverse_projection,
                                             bool orthographic,
                                             float near_plane, float far_plane,
                                             i32 tile_x, i32 tile_y,
                                             i32 tile_z) {
  return gl::build_cluster_record(config, inverse_projection,
                                       orthographic, near_plane, far_plane,
                                       tile_x, tile_y, tile_z);
}

} // namespace ecs
