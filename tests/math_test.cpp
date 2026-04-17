#include <gtest/gtest.h>

#include "rydz_math/mod.hpp"

namespace math = rydz_math;

TEST(MathTest, Vec3UsesGlamStyleMethods) {
  math::Vec3 zero = math::Vec3::ZERO;
  math::Vec3 value = math::Vec3::splat(2.0F);

  EXPECT_FLOAT_EQ(zero.x, 0.0F);
  EXPECT_FLOAT_EQ(zero.y, 0.0F);
  EXPECT_FLOAT_EQ(zero.z, 0.0F);
  EXPECT_FLOAT_EQ(value.length_sq(), 12.0F);
  EXPECT_FLOAT_EQ(value.dot(math::Vec3(1.0F, 0.0F, 0.0F)), 2.0F);

  math::Vec3 cross = math::Vec3(1.0F, 0.0F, 0.0F).cross(
      math::Vec3(0.0F, 1.0F, 0.0F));
  EXPECT_FLOAT_EQ(cross.x, 0.0F);
  EXPECT_FLOAT_EQ(cross.y, 0.0F);
  EXPECT_FLOAT_EQ(cross.z, 1.0F);
}

TEST(MathTest, Mat4UsesGlamStyleMethods) {
  math::Mat4 transform = math::Mat4::from_rotation_translation(
      math::Quat::IDENTITY, math::Vec3(1.0F, 2.0F, 3.0F));

  EXPECT_FLOAT_EQ(transform.translation().x, 1.0F);
  EXPECT_FLOAT_EQ(transform.translation().y, 2.0F);
  EXPECT_FLOAT_EQ(transform.translation().z, 3.0F);

  transform.set_translation(math::Vec3::ZERO);
  EXPECT_FLOAT_EQ(transform.translation().x, 0.0F);
  EXPECT_FLOAT_EQ(transform.translation().y, 0.0F);
  EXPECT_FLOAT_EQ(transform.translation().z, 0.0F);
}
