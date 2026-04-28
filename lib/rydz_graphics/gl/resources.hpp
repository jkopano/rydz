#pragma once

#include "rydz_graphics/gl/mesh.hpp"
#include "rydz_graphics/gl/primitives.hpp"
#include "rydz_graphics/gl/textures.hpp"
#include <string>

namespace gl {

inline auto load_image(char const* path) -> Image { return rl::rlLoadImage(path); }

inline auto load_image(std::string const& path) -> Image {
  return load_image(path.c_str());
}

inline auto unload_image(Image& image) -> void { image.unload(); }

inline auto image_format(Image& image, i32 format) -> void { image.format_to(format); }

inline auto gen_image_with_color(i32 width, i32 height, ecs::Color color) -> Image {
  return rl::GenImageColor(width, height, color);
}

inline auto load_texture_from_image(Image const& image) -> Texture {
  return image.load_texture();
}

inline auto unload_texture_id(u32 id) -> void { rl::rlUnloadTexture(id); }

inline auto set_vertex_attribute(
  u32 index, i32 component_count, i32 type, bool normalized, i32 stride, i32 offset
) -> void {
  rl::rlSetVertexAttribute(index, component_count, type, normalized, stride, offset);
}

inline auto enable_vertex_attribute(u32 index) -> void {
  rl::rlEnableVertexAttribute(index);
}

inline auto active_texture_slot(i32 slot) -> void { rl::rlActiveTextureSlot(slot); }

inline auto enable_texture_cubemap(u32 cubemap_id) -> void {
  rl::rlEnableTextureCubemap(cubemap_id);
}

inline auto disable_texture_cubemap() -> void { rl::rlDisableTextureCubemap(); }

inline auto load_texture_cubemap(void const* data, i32 size, i32 format, i32 mipmaps)
  -> u32 {
  return rl::rlLoadTextureCubemap(data, size, format, mipmaps);
}

} // namespace gl
