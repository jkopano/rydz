#include <gtest/gtest.h>

#include "rydz_graphics/gl/buffers.hpp"
#include <type_traits>

// Buffer abstract base class has been removed (Requirement 6.1).
// SSBO and UBO are now independent final types (Requirements 6.2, 6.3).
static_assert(!std::is_abstract_v<gl::SSBO>);
static_assert(!std::is_abstract_v<gl::UBO>);

// Both types are final with no base class.
static_assert(std::is_final_v<gl::SSBO>);
static_assert(std::is_final_v<gl::UBO>);

// Move semantics preserved (Requirement 6.5).
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

// GpuBuffer concept satisfaction (Requirement 6.4 / Property 8).
// **Validates: Requirements 6.4**
static_assert(gl::GpuBuffer<gl::SSBO>);
static_assert(gl::GpuBuffer<gl::UBO>);

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
}

// Property 2: SSBO/UBO move leaves source empty (Requirement 6.5).
// **Validates: Requirements 6.5**
TEST(GraphicsBufferTest, MoveConstructorLeavesSourceEmpty) {
  // Default-constructed (id == 0) — move should keep source at 0.
  gl::SSBO ssbo_src;
  gl::SSBO ssbo_dst{std::move(ssbo_src)};
  EXPECT_EQ(ssbo_src.id(), 0u); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(ssbo_dst.id(), 0u);

  gl::UBO ubo_src;
  gl::UBO ubo_dst{std::move(ubo_src)};
  EXPECT_EQ(ubo_src.id(), 0u); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(ubo_dst.id(), 0u);
}

TEST(GraphicsBufferTest, MoveAssignmentLeavesSourceEmpty) {
  gl::SSBO ssbo_src;
  gl::SSBO ssbo_dst;
  ssbo_dst = std::move(ssbo_src);
  EXPECT_EQ(ssbo_src.id(), 0u); // NOLINT(bugprone-use-after-move)

  gl::UBO ubo_src;
  gl::UBO ubo_dst;
  ubo_dst = std::move(ubo_src);
  EXPECT_EQ(ubo_src.id(), 0u); // NOLINT(bugprone-use-after-move)
}
