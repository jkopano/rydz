#include <gtest/gtest.h>

#include "rydz_graphics/lighting/clustered_lighting.hpp"
#include "rydz_graphics/lighting/shadow.hpp"

TEST(ClusteredLightingTest, ConfigClampsInvalidDimensions) {
  gl::ClusterConfig config{
    .tile_count_x = 0,
    .tile_count_y = -4,
    .slice_count_z = 0,
    .max_point_lights = 0,
    .max_lights_per_cluster = 0,
  };

  EXPECT_EQ(config.tile_count_x_clamped(), 1);
  EXPECT_EQ(config.tile_count_y_clamped(), 1);
  EXPECT_EQ(config.slice_count_z_clamped(), 1);
  EXPECT_EQ(config.max_lights_per_cluster_clamped(), 1U);
  EXPECT_EQ(config.cluster_count(), 1U);
  EXPECT_EQ(config.max_light_indices(), 1U);
}

TEST(ClusteredLightingTest, ClusterRecordUsesClampedStrideAndClearsCount) {
  gl::ClusterConfig config{
    .tile_count_x = 2,
    .tile_count_y = 2,
    .slice_count_z = 2,
    .max_point_lights = 16,
    .max_lights_per_cluster = 4,
  };
  math::Mat4 const projection =
    math::Mat4::orthographic_rh(-1.0F, 1.0F, -1.0F, 1.0F, 1.0F, 9.0F);

  gl::ClusterGpuRecord const record = gl::build_cluster_record(
    config,
    projection.inverse(),
    true,
    1.0F,
    9.0F,
    1,
    1,
    1
  );

  EXPECT_EQ(record.light_index_offset, 28U);
  EXPECT_EQ(record.light_count, 0U);
  EXPECT_EQ(record._pad0, 0U);
  EXPECT_EQ(record._pad1, 0U);
  EXPECT_LT(record.min_bounds.x, record.max_bounds.x);
  EXPECT_LT(record.min_bounds.y, record.max_bounds.y);
  EXPECT_LT(record.min_bounds.z, record.max_bounds.z);
}

TEST(ClusteredLightingTest, PointShadowedLightCountDefaultsWithinLimits) {
  ecs::ShadowSettings settings{};

  EXPECT_GE(settings.max_shadowed_point_lights_clamped(), 0);
  EXPECT_LE(
    settings.max_shadowed_point_lights_clamped(),
    ecs::MAX_POINT_SHADOWS
  );
}

TEST(ClusteredLightingTest, PointShadowCountCanBeClampedToZero) {
  ecs::ShadowSettings settings{};
  settings.max_shadowed_point_lights = 0;

  EXPECT_EQ(settings.max_shadowed_point_lights_clamped(), 0);
}

TEST(ClusteredLightingTest, LocalLightGpuLayoutRemainsStd430Aligned) {
  EXPECT_EQ(sizeof(gl::GpuLocalLight), 64U);
}
