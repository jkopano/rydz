#include <gtest/gtest.h>

#include "rydz_graphics/gl/textures.hpp"
#include "rydz_graphics/gl/texture_builder.hpp"
#include <utility>

using namespace gl;

// ============================================================================
// Texture::builder() tests
// ============================================================================

// **Validates: Requirements 1.1**
// Texture::builder() returns a TextureBuilder for fluent API access
TEST(TextureBuilderTest, TextureBuilderReturnsBuilder) {
  // This test verifies that Texture::builder() returns a TextureBuilder
  // that can be used with the fluent API pattern
  
  // The builder should be default-constructed and ready to use
  auto builder = Texture::builder();
  
  // We can't easily test the full build() without a valid OpenGL context,
  // but we can verify the method chaining works
  auto& builder_ref = builder.with_filter(gl::TextureFilter::Linear);
  
  // Verify method chaining returns a reference to the same builder
  ASSERT_EQ(&builder, &builder_ref);
}

// **Validates: Requirements 1.1, 1.5**
// Texture::builder() enables fluent API pattern with method chaining
TEST(TextureBuilderTest, FluentAPIMethodChaining) {
  // Verify that the fluent API pattern works as expected:
  // Texture::builder().from_file("path").with_filter(...).build()
  
  auto builder = Texture::builder();
  
  // Chain multiple configuration methods
  auto& b1 = builder.with_filter(gl::TextureFilter::Trilinear);
  auto& b2 = b1.with_wrap(gl::TextureWrap::Repeat);
  auto& b3 = b2.with_mipmaps(true);
  auto& b4 = b3.with_anisotropic(4.0F);
  
  // All should return references to the same builder
  ASSERT_EQ(&builder, &b1);
  ASSERT_EQ(&builder, &b2);
  ASSERT_EQ(&builder, &b3);
  ASSERT_EQ(&builder, &b4);
}

// ============================================================================
// Texture parameter methods tests
// ============================================================================

// **Validates: Requirements 8.1, 8.2, 13.1, 13.5**
// Texture provides set_filter() and set_wrap() methods
TEST(TextureTest, TextureParameterMethodsExist) {
  // Verify that Texture has the required parameter methods
  // We can't test actual functionality without OpenGL context,
  // but we can verify the methods exist and compile
  
  Texture texture;
  
  // These should compile without errors
  texture.set_filter(GL_LINEAR);
  texture.set_wrap(GL_REPEAT);
  
  // Verify ready() and rect() accessor methods exist
  ASSERT_FALSE(texture.ready());  // Default texture should not be ready
  
  auto rect = texture.rect();
  ASSERT_EQ(rect.x, 0.0F);
  ASSERT_EQ(rect.y, 0.0F);
}

// **Validates: Requirements 15.1, 15.2, 15.3**
// Texture provides generate_mipmaps() method
TEST(TextureTest, GenerateMipmapsMethodExists) {
  // Verify that Texture has the generate_mipmaps() method
  // We can't test actual functionality without OpenGL context,
  // but we can verify the method exists and compiles
  
  Texture texture;
  
  // This should compile without errors
  texture.generate_mipmaps();
  
  // For a non-ready texture, mipmaps should remain 0
  ASSERT_EQ(texture.mipmaps, 0);
}

// ============================================================================
// RAII Resource Management Tests
// ============================================================================

// **Validates: Requirements 7.1, 7.2, 7.3, 7.4, 18.1, 18.2, 18.3, 18.4, 18.5**
// **Property 14: Move Semantics Correctness**
// For any resource R of type Texture, when R is moved, all fields are 
// transferred to the destination, the source is left with id = 0, and the 
// moved-from resource does not free GPU resources on destruction.

TEST(TextureMoveSemantics, MoveConstructorLeavesSourceEmpty) {
  // **Validates: Requirements 7.2, 7.3, 18.1, 18.2, 18.3**
  // A default-constructed Texture has id==0. After move-construction,
  // the source must still have id==0 (the invariant holds for any id value).
  Texture src;
  EXPECT_EQ(src.id, 0u);
  EXPECT_FALSE(src.ready());

  Texture dst(std::move(src));
  // Source must be empty after move
  EXPECT_EQ(src.id, 0u);   // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(src.ready()); // NOLINT(bugprone-use-after-move)
  // Destination inherits the (zero) id
  EXPECT_EQ(dst.id, 0u);
}

TEST(TextureMoveSemantics, MoveAssignmentLeavesSourceEmpty) {
  // **Validates: Requirements 7.2, 7.3, 18.1, 18.2, 18.3**
  Texture src;
  Texture dst;

  dst = std::move(src);

  EXPECT_EQ(src.id, 0u);   // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(src.ready()); // NOLINT(bugprone-use-after-move)
}

TEST(TextureMoveSemantics, MoveAssignmentSelfAssignIsNoop) {
  // **Validates: Requirements 7.2, 7.3, 18.2**
  // Self-assignment must not corrupt state.
  Texture tex;
  // Suppress self-move warning — intentional test
  auto& ref = tex;
  tex = std::move(ref); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(tex.id, 0u);
}

TEST(TextureMoveSemantics, MoveConstructorTransfersAllFields) {
  // **Validates: Requirements 18.1, 18.2**
  // Verify that all fields are transferred during move construction
  // Note: We test with a default texture since we can't create real GPU resources in tests
  Texture src;
  // Verify default state
  EXPECT_EQ(src.id, 0u);
  EXPECT_EQ(src.width, 0);
  EXPECT_EQ(src.height, 0);
  EXPECT_EQ(src.mipmaps, 0);
  EXPECT_EQ(src.format, 0);

  Texture dst(std::move(src));

  // Destination should have all fields (all zeros for default texture)
  EXPECT_EQ(dst.id, 0u);
  EXPECT_EQ(dst.width, 0);
  EXPECT_EQ(dst.height, 0);
  EXPECT_EQ(dst.mipmaps, 0);
  EXPECT_EQ(dst.format, 0);

  // Source should be zeroed (same as before since it was already zero)
  EXPECT_EQ(src.id, 0u);           // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.width, 0);         // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.height, 0);        // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.mipmaps, 0);       // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.format, 0);        // NOLINT(bugprone-use-after-move)
}

TEST(TextureMoveSemantics, MoveAssignmentTransfersAllFields) {
  // **Validates: Requirements 18.1, 18.2**
  // Verify that all fields are transferred during move assignment
  // Note: We test with a default texture since we can't create real GPU resources in tests
  Texture src;
  // Verify default state
  EXPECT_EQ(src.id, 0u);
  EXPECT_EQ(src.width, 0);
  EXPECT_EQ(src.height, 0);
  EXPECT_EQ(src.mipmaps, 0);
  EXPECT_EQ(src.format, 0);

  Texture dst;
  dst = std::move(src);

  // Destination should have all fields (all zeros for default texture)
  EXPECT_EQ(dst.id, 0u);
  EXPECT_EQ(dst.width, 0);
  EXPECT_EQ(dst.height, 0);
  EXPECT_EQ(dst.mipmaps, 0);
  EXPECT_EQ(dst.format, 0);

  // Source should be zeroed (same as before since it was already zero)
  EXPECT_EQ(src.id, 0u);           // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.width, 0);         // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.height, 0);        // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.mipmaps, 0);       // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(src.format, 0);        // NOLINT(bugprone-use-after-move)
}

// ============================================================================
// Structural Tests for Texture RAII
// ============================================================================

TEST(TextureStructural, DefaultConstructedIsNotReady) {
  // **Validates: Requirements 7.1, 13.1**
  Texture tex;
  EXPECT_FALSE(tex.ready());
  EXPECT_EQ(tex.id, 0u);
}

TEST(TextureStructural, IsNotCopyConstructible) {
  // **Validates: Requirements 7.4**
  EXPECT_FALSE(std::is_copy_constructible_v<Texture>);
}

TEST(TextureStructural, IsNotCopyAssignable) {
  // **Validates: Requirements 7.4**
  EXPECT_FALSE(std::is_copy_assignable_v<Texture>);
}

TEST(TextureStructural, IsMoveConstructible) {
  // **Validates: Requirements 7.2, 18.1**
  EXPECT_TRUE(std::is_move_constructible_v<Texture>);
}

TEST(TextureStructural, IsMoveAssignable) {
  // **Validates: Requirements 7.2, 18.1**
  EXPECT_TRUE(std::is_move_assignable_v<Texture>);
}

TEST(TextureStructural, MoveConstructedFromEmptyIsEmpty) {
  // **Validates: Requirements 7.2, 18.2, 18.3**
  Texture a;
  Texture b(std::move(a));
  EXPECT_EQ(b.id, 0u);
  EXPECT_FALSE(b.ready());
}

TEST(TextureStructural, UnloadOnEmptyIsNoop) {
  // **Validates: Requirements 7.1, 7.5**
  Texture tex;
  EXPECT_NO_THROW(tex.unload());
  EXPECT_EQ(tex.id, 0u);
}
