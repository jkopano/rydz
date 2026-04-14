#include <gtest/gtest.h>

#include "rydz_graphics/gl/core.hpp"
#include <type_traits>

static_assert(std::is_abstract_v<gl::Buffer>);
static_assert(std::has_virtual_destructor_v<gl::Buffer>);
static_assert(!std::is_copy_constructible_v<gl::Buffer>);
static_assert(!std::is_copy_assignable_v<gl::Buffer>);

static_assert(std::is_base_of_v<gl::Buffer, gl::SSBO>);
static_assert(std::is_base_of_v<gl::Buffer, gl::UBO>);

static_assert(!std::is_copy_constructible_v<gl::SSBO>);
static_assert(!std::is_copy_assignable_v<gl::SSBO>);
static_assert(std::is_move_constructible_v<gl::SSBO>);
static_assert(std::is_move_assignable_v<gl::SSBO>);

static_assert(!std::is_copy_constructible_v<gl::UBO>);
static_assert(!std::is_copy_assignable_v<gl::UBO>);
static_assert(std::is_move_constructible_v<gl::UBO>);
static_assert(std::is_move_assignable_v<gl::UBO>);

TEST(GraphicsBufferTest, DefaultBuffersAreEmpty) {
  gl::SSBO ssbo;
  gl::UBO ubo;

  EXPECT_FALSE(ssbo.ready());
  EXPECT_FALSE(ubo.ready());

  EXPECT_EQ(ssbo.id(), 0u);
  EXPECT_EQ(ubo.id(), 0u);

  const gl::Buffer &base = ssbo;
  EXPECT_FALSE(base.ready());
  EXPECT_EQ(base.id(), 0u);
}
