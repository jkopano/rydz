// Example demonstrating Texture::builder() usage
// This example shows the fluent API pattern for texture creation

#include "rydz_graphics/gl/textures.hpp"
#include "rydz_graphics/gl/texture_builder.hpp"

int main() {
  // Example 1: Using Texture::builder() with fluent API
  // This is the new pattern enabled by Requirement 1.1
  auto texture1 = gl::Texture::builder()
    .from_file("assets/textures/diffuse.png")
    .with_filter(gl::TextureFilter::Trilinear)
    .with_wrap(gl::TextureWrap::Repeat)
    .with_mipmaps(true)
    .build();

  // Example 2: Alternative pattern - still using TextureBuilder directly
  auto texture2 = gl::TextureBuilder::from_file("assets/textures/normal.png")
    .with_filter(gl::TextureFilter::Linear)
    .with_wrap(gl::TextureWrap::Clamp)
    .build();

  // Example 3: Creating an empty texture with builder
  auto render_texture = gl::Texture::builder()
    .empty(1920, 1080, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    .with_filter(gl::TextureFilter::Linear)
    .build();

  // Clean up
  texture1.unload();
  texture2.unload();
  render_texture.unload();

  return 0;
}
