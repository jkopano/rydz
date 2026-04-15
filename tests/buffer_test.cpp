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

static_assert(!std::is_copy_constructible_v<gl::VAO>);
static_assert(!std::is_copy_assignable_v<gl::VAO>);
static_assert(std::is_move_constructible_v<gl::VAO>);
static_assert(std::is_move_assignable_v<gl::VAO>);

static_assert(!std::is_copy_constructible_v<gl::VBO>);
static_assert(!std::is_copy_assignable_v<gl::VBO>);
static_assert(std::is_move_constructible_v<gl::VBO>);
static_assert(std::is_move_assignable_v<gl::VBO>);

static_assert(!std::is_copy_constructible_v<gl::EBO>);
static_assert(!std::is_copy_assignable_v<gl::EBO>);
static_assert(std::is_move_constructible_v<gl::EBO>);
static_assert(std::is_move_assignable_v<gl::EBO>);

TEST(GraphicsBufferTest, DefaultBuffersAreEmpty) {
  gl::SSBO ssbo;
  gl::UBO ubo;
  gl::VAO vertex_array;
  gl::VBO vertex_buffer;
  gl::EBO element_buffer;

  EXPECT_FALSE(ssbo.ready());
  EXPECT_FALSE(ubo.ready());
  EXPECT_FALSE(vertex_array.ready());
  EXPECT_FALSE(vertex_buffer.ready());
  EXPECT_FALSE(element_buffer.ready());

  EXPECT_EQ(ssbo.id(), 0u);
  EXPECT_EQ(ubo.id(), 0u);
  EXPECT_EQ(vertex_array.id(), 0u);
  EXPECT_EQ(vertex_buffer.id(), 0u);
  EXPECT_EQ(element_buffer.id(), 0u);

  const gl::Buffer &base = ssbo;
  EXPECT_FALSE(base.ready());
  EXPECT_EQ(base.id(), 0u);
}
