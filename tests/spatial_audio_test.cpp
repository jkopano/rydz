#include <gtest/gtest.h>

#include "rydz_audio/spatial.hpp"
#include "rydz_math/mod.hpp"

namespace audio = rydz_audio;
namespace math = rydz_math;

// ============================================================================
// calculate_attenuation Tests
// ============================================================================

TEST(SpatialAudioTest, AttenuationBelowMinDistance) {
  // When distance is below min_distance, attenuation should be 1.0 (no attenuation)
  f32 attenuation = audio::calculate_attenuation(
    0.5f,  // distance
    1.0f,  // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Linear
  );
  
  EXPECT_FLOAT_EQ(attenuation, 1.0f);
}

TEST(SpatialAudioTest, AttenuationAboveMaxDistance) {
  // When distance is above max_distance, attenuation should be 0.0 (silence)
  f32 attenuation = audio::calculate_attenuation(
    150.0f, // distance
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Linear
  );
  
  EXPECT_FLOAT_EQ(attenuation, 0.0f);
}

TEST(SpatialAudioTest, AttenuationLinearModel) {
  // Linear attenuation at midpoint should be 0.5
  f32 attenuation = audio::calculate_attenuation(
    50.5f,  // distance (midpoint between 1 and 100)
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Linear
  );
  
  EXPECT_NEAR(attenuation, 0.5f, 0.01f);
}

TEST(SpatialAudioTest, AttenuationInverseDistanceModel) {
  // Inverse distance: attenuation = min_dist / distance
  f32 attenuation = audio::calculate_attenuation(
    10.0f,  // distance
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::InverseDistance
  );
  
  EXPECT_FLOAT_EQ(attenuation, 0.1f); // 1.0 / 10.0
}

TEST(SpatialAudioTest, AttenuationExponentialModel) {
  // Exponential attenuation at midpoint
  f32 attenuation = audio::calculate_attenuation(
    50.5f,  // distance (midpoint)
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Exponential
  );
  
  // At midpoint, t = 0.5, exponential = (1 - 0.5)^2 = 0.25
  EXPECT_NEAR(attenuation, 0.25f, 0.01f);
}

// ============================================================================
// calculate_stereo_panning Tests
// ============================================================================

TEST(SpatialAudioTest, PanningSourceAtListenerPosition) {
  // When source is at listener position, panning should be 0.0 (center)
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  f32 panning = audio::calculate_stereo_panning(
    source_pos,
    listener_pos,
    forward,
    up
  );
  
  EXPECT_FLOAT_EQ(panning, 0.0f);
}

TEST(SpatialAudioTest, PanningSourceToTheRight) {
  // Source to the right should have positive panning
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(10.0f, 0.0f, 0.0f); // To the right
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  f32 panning = audio::calculate_stereo_panning(
    source_pos,
    listener_pos,
    forward,
    up
  );
  
  EXPECT_GT(panning, 0.0f);
  EXPECT_LE(panning, 1.0f);
}

TEST(SpatialAudioTest, PanningSourceToTheLeft) {
  // Source to the left should have negative panning
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(-10.0f, 0.0f, 0.0f); // To the left
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  f32 panning = audio::calculate_stereo_panning(
    source_pos,
    listener_pos,
    forward,
    up
  );
  
  EXPECT_LT(panning, 0.0f);
  EXPECT_GE(panning, -1.0f);
}

TEST(SpatialAudioTest, PanningSourceInFront) {
  // Source directly in front should have near-zero panning
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(0.0f, 0.0f, -10.0f); // In front
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  f32 panning = audio::calculate_stereo_panning(
    source_pos,
    listener_pos,
    forward,
    up
  );
  
  EXPECT_NEAR(panning, 0.0f, 0.01f);
}

TEST(SpatialAudioTest, PanningSourceBehind) {
  // Source directly behind should have near-zero panning
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(0.0f, 0.0f, 10.0f); // Behind
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  f32 panning = audio::calculate_stereo_panning(
    source_pos,
    listener_pos,
    forward,
    up
  );
  
  EXPECT_NEAR(panning, 0.0f, 0.01f);
}

// ============================================================================
// apply_spatial_audio Tests
// ============================================================================

TEST(SpatialAudioTest, ApplySpatialAudioCombinesAttenuationAndPanning) {
  // Test that apply_spatial_audio correctly combines attenuation and panning
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(5.0f, 0.0f, 0.0f); // To the right, distance = 5
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  auto [attenuation, panning] = audio::apply_spatial_audio(
    source_pos,
    listener_pos,
    forward,
    up,
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Linear
  );
  
  // Attenuation should be calculated based on distance
  EXPECT_GT(attenuation, 0.0f);
  EXPECT_LE(attenuation, 1.0f);
  
  // Panning should be positive (to the right)
  EXPECT_GT(panning, 0.0f);
  EXPECT_LE(panning, 1.0f);
}

TEST(SpatialAudioTest, ApplySpatialAudioWithInverseDistance) {
  // Test with inverse distance model
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(0.0f, 0.0f, -10.0f); // In front, distance = 10
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  auto [attenuation, panning] = audio::apply_spatial_audio(
    source_pos,
    listener_pos,
    forward,
    up,
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::InverseDistance
  );
  
  // Attenuation should be 1.0 / 10.0 = 0.1
  EXPECT_FLOAT_EQ(attenuation, 0.1f);
  
  // Panning should be near zero (in front)
  EXPECT_NEAR(panning, 0.0f, 0.01f);
}

TEST(SpatialAudioTest, ApplySpatialAudioBeyondMaxDistance) {
  // Test that sources beyond max distance are silent
  math::Vec3 listener_pos(0.0f, 0.0f, 0.0f);
  math::Vec3 source_pos(150.0f, 0.0f, 0.0f); // Far to the right
  math::Vec3 forward(0.0f, 0.0f, -1.0f);
  math::Vec3 up(0.0f, 1.0f, 0.0f);
  
  auto [attenuation, panning] = audio::apply_spatial_audio(
    source_pos,
    listener_pos,
    forward,
    up,
    1.0f,   // min_distance
    100.0f, // max_distance
    audio::AttenuationModel::Linear
  );
  
  // Attenuation should be 0.0 (silence)
  EXPECT_FLOAT_EQ(attenuation, 0.0f);
  
  // Panning is still calculated (though volume is 0)
  EXPECT_GT(panning, 0.0f);
}
