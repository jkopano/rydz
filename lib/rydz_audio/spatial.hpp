#pragma once

#include "rydz_math/mod.hpp"
#include "types.hpp"
#include <cmath>

namespace rydz_audio {

[[nodiscard]] inline auto calculate_attenuation(
  f32 distance, f32 min_dist, f32 max_dist, AttenuationModel model
) -> f32 {
  if (distance <= min_dist)
    return 1.0f;
  if (distance >= max_dist)
    return 0.0f;

  f32 t = (distance - min_dist) / (max_dist - min_dist);

  switch (model) {
  case AttenuationModel::Linear:
    return 1.0f - t;

  case AttenuationModel::InverseDistance:
    return min_dist / distance;

  case AttenuationModel::Exponential:
    return std::pow(1.0f - t, 2.0f);
  }

  return 1.0f;
}

[[nodiscard]] inline auto calculate_stereo_panning(
  rydz_math::Vec3 const& source_pos,
  rydz_math::Vec3 const& listener_pos,
  rydz_math::Vec3 const& listener_forward,
  rydz_math::Vec3 const& listener_up
) -> f32 {

  rydz_math::Vec3 to_source = source_pos - listener_pos;

  f32 distance_sq = to_source.length_sq();
  if (distance_sq < 0.0001f) {
    return 0.0f;
  }

  to_source = to_source.normalized();

  rydz_math::Vec3 right = listener_forward.cross(listener_up);
  right = right.normalized();

  f32 pan = to_source.dot(right);

  return std::clamp(pan, -1.0f, 1.0f);
}

[[nodiscard]] inline auto apply_spatial_audio(
  rydz_math::Vec3 const& source_pos,
  rydz_math::Vec3 const& listener_pos,
  rydz_math::Vec3 const& listener_forward,
  rydz_math::Vec3 const& listener_up,
  f32 min_dist,
  f32 max_dist,
  AttenuationModel model
) -> std::pair<f32, f32> {

  rydz_math::Vec3 diff = source_pos - listener_pos;
  f32 distance = diff.length();

  f32 attenuation = calculate_attenuation(distance, min_dist, max_dist, model);

  f32 panning =
    calculate_stereo_panning(source_pos, listener_pos, listener_forward, listener_up);

  return {attenuation, panning};
}

} // namespace rydz_audio
