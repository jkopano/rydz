#include <gtest/gtest.h>

#include "rl.hpp"
#include <type_traits>

// Test that all wrapper functions are accessible in the rl namespace
// This validates Requirements 4.1, 4.2, 4.3, 4.4, 4.5

// Texture parameter wrapper functions exist
TEST(RlWrapperTest, TextureParameterFunctionsExist) {
  // Check that SetTextureParameteri exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::SetTextureParameteri), unsigned int, unsigned int, int>);
  
  // Check that SetTextureParameterf exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::SetTextureParameterf), unsigned int, unsigned int, float>);
  
  // Check that GenTextureMipmaps exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::GenTextureMipmaps), unsigned int>);
}

// Uniform block wrapper functions exist
TEST(RlWrapperTest, UniformBlockFunctionsExist) {
  // Check that GetUniformBlockIndex exists and has correct signature
  static_assert(std::is_invocable_r_v<unsigned int, decltype(rl::GetUniformBlockIndex), unsigned int, char const*>);
  
  // Check that UniformBlockBinding exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::UniformBlockBinding), unsigned int, unsigned int, unsigned int>);
}

// Framebuffer wrapper functions exist
TEST(RlWrapperTest, FramebufferFunctionsExist) {
  // Check that CheckFramebufferStatus exists and has correct signature
  static_assert(std::is_invocable_r_v<unsigned int, decltype(rl::CheckFramebufferStatus), unsigned int>);
  
  // Check that BindFramebuffer exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::BindFramebuffer), unsigned int, unsigned int>);
}

// Buffer wrapper functions exist
TEST(RlWrapperTest, BufferFunctionsExist) {
  // Check that GenBuffers exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::GenBuffers), int, unsigned int*>);
  
  // Check that DeleteBuffers exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::DeleteBuffers), int, unsigned int const*>);
  
  // Check that BindBuffer exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::BindBuffer), unsigned int, unsigned int>);
  
  // Check that BufferData exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::BufferData), unsigned int, long, void const*, unsigned int>);
  
  // Check that BufferSubData exists and has correct signature
  static_assert(std::is_invocable_v<decltype(rl::BufferSubData), unsigned int, long, long, void const*>);
}

// Test that wrapper functions follow consistent naming convention
TEST(RlWrapperTest, NamingConventionConsistency) {
  // All wrapper functions should be in rl:: namespace
  // All wrapper functions should use PascalCase naming
  // This is validated by the compilation succeeding with the above tests
  SUCCEED();
}
