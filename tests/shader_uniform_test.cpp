#include <gtest/gtest.h>

#include "rydz_graphics/gl/shader.hpp"
#include <functional>

TEST(ShaderUniformTest, IntVector3ConstructorStoresAllComponents) {
  gl::Uniform uniform{1, 2, 3};

  EXPECT_EQ(uniform.type, gl::UniformType::IVec3);
  EXPECT_EQ(uniform.count, 1);
  EXPECT_EQ(uniform.int_data[0], 1);
  EXPECT_EQ(uniform.int_data[1], 2);
  EXPECT_EQ(uniform.int_data[2], 3);
}

TEST(ShaderUniformTest, IntVector4ConstructorStoresAllComponents) {
  gl::Uniform uniform{1, 2, 3, 4};

  EXPECT_EQ(uniform.type, gl::UniformType::IVec4);
  EXPECT_EQ(uniform.count, 1);
  EXPECT_EQ(uniform.int_data[0], 1);
  EXPECT_EQ(uniform.int_data[1], 2);
  EXPECT_EQ(uniform.int_data[2], 3);
  EXPECT_EQ(uniform.int_data[3], 4);
}

TEST(ShaderUniformTest, EqualIntegerUniformsHaveEqualHashes) {
  gl::Uniform a{1, 2, 3, 4};
  gl::Uniform b{1, 2, 3, 4};

  ASSERT_EQ(a, b);
  EXPECT_EQ(std::hash<gl::Uniform>{}(a), std::hash<gl::Uniform>{}(b));
}

TEST(ShaderUniformTest, EqualFloatUniformsHaveEqualHashes) {
  gl::Uniform a{1.0f, 2.0f, 3.0f};
  gl::Uniform b{1.0f, 2.0f, 3.0f};

  ASSERT_EQ(a, b);
  EXPECT_EQ(std::hash<gl::Uniform>{}(a), std::hash<gl::Uniform>{}(b));
}
