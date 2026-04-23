#include <gtest/gtest.h>

#include "rydz_graphics/gl/primitives.hpp"

TEST(RectangleTest, ConvertsToAndFromRaylibRectangle) {
  ::rlRectangle raw{1.0F, 2.0F, 3.0F, 4.0F};

  gl::Rectangle rect{raw};
  ::rlRectangle converted = rect;

  EXPECT_FLOAT_EQ(rect.x, 1.0F);
  EXPECT_FLOAT_EQ(rect.y, 2.0F);
  EXPECT_FLOAT_EQ(rect.width, 3.0F);
  EXPECT_FLOAT_EQ(rect.height, 4.0F);
  EXPECT_FLOAT_EQ(converted.x, raw.x);
  EXPECT_FLOAT_EQ(converted.y, raw.y);
  EXPECT_FLOAT_EQ(converted.width, raw.width);
  EXPECT_FLOAT_EQ(converted.height, raw.height);
}

TEST(RectangleTest, NormalizesNegativeSize) {
  gl::Rectangle rect{10.0F, 20.0F, -4.0F, -6.0F};

  gl::Rectangle normalized = rect.normalized();

  EXPECT_FLOAT_EQ(normalized.x, 6.0F);
  EXPECT_FLOAT_EQ(normalized.y, 14.0F);
  EXPECT_FLOAT_EQ(normalized.width, 4.0F);
  EXPECT_FLOAT_EQ(normalized.height, 6.0F);
}

TEST(RectangleTest, ComputesContainmentAndIntersection) {
  gl::Rectangle a{0.0F, 0.0F, 10.0F, 10.0F};
  gl::Rectangle b{5.0F, 6.0F, 10.0F, 10.0F};

  EXPECT_TRUE(a.contains({5.0F, 5.0F}));
  EXPECT_FALSE(a.contains({12.0F, 5.0F}));
  EXPECT_TRUE(a.overlaps(b));

  gl::Rectangle intersection = a.intersection(b);
  EXPECT_FLOAT_EQ(intersection.x, 5.0F);
  EXPECT_FLOAT_EQ(intersection.y, 6.0F);
  EXPECT_FLOAT_EQ(intersection.width, 5.0F);
  EXPECT_FLOAT_EQ(intersection.height, 4.0F);
}
