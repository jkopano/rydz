#pragma once

#include "rydz_graphics/gl/core.hpp"
#include <string>

namespace gl {

inline auto load_image(const char *path) -> Image {
  return rl::rlLoadImage(path);
}

inline auto load_image(const std::string &path) -> Image {
  return load_image(path.c_str());
}

inline auto unload_image(Image &image) -> void { image.unload(); }

inline auto image_format(Image &image, i32 format) -> void {
  image.format_to(format);
}

inline auto gen_image_with_color(i32 width, i32 height, Color color) -> Image {
  return rl::GenImageColor(width, height, color);
}

inline auto load_texture(const char *path) -> Texture {
  return rl::LoadTexture(path);
}

inline auto load_texture(const std::string &path) -> Texture {
  return load_texture(path.c_str());
}

inline auto load_texture_from_image(const Image &image) -> Texture {
  return image.load_texture();
}

inline auto unload_texture_id(u32 id) -> void { rl::rlUnloadTexture(id); }

inline auto load_sound(const char *path) -> Sound {
  return rl::LoadSound(path);
}

inline auto load_sound(const std::string &path) -> Sound {
  return load_sound(path.c_str());
}

inline auto unload_sound(Sound &sound) -> void { sound.unload(); }

inline auto gen_cube(float width = 1.0F, float height = 1.0F,
                     float length = 1.0F) -> Mesh {
  return rl::GenMeshCube(width, height, length);
}

inline auto gen_sphere(float radius = 0.5F, i32 rings = 16, i32 slices = 16)
    -> Mesh {
  return rl::GenMeshSphere(radius, rings, slices);
}

inline auto gen_plane(float width = 10.0F, float length = 10.0F, i32 res_x = 1,
                      i32 res_z = 1) -> Mesh {
  return rl::GenMeshPlane(width, length, res_x, res_z);
}

inline auto gen_cylinder(float radius = 0.5F, float height = 1.0F,
                         i32 slices = 16) -> Mesh {
  return rl::GenMeshCylinder(radius, height, slices);
}

inline auto gen_torus(float radius = 0.5F, float size = 0.2F,
                      i32 radial_segments = 16, i32 sides = 16) -> Mesh {
  return rl::GenMeshTorus(radius, size, radial_segments, sides);
}

inline auto gen_hemisphere(float radius = 0.5f, i32 rings = 16, i32 slices = 16)
    -> Mesh {
  return rl::GenMeshHemiSphere(radius, rings, slices);
}

inline auto gen_knot(float radius = 0.5F, float size = 0.2F,
                     i32 radial_segments = 16, i32 sides = 16) -> Mesh {
  return rl::GenMeshKnot(radius, size, radial_segments, sides);
}

inline auto gen_tangents(Mesh &mesh) -> void { mesh.gen_tangents(); }

inline auto set_vertex_attribute(u32 index, i32 component_count, i32 type,
                                 bool normalized, i32 stride, i32 offset)
    -> void {
  rl::rlSetVertexAttribute(index, component_count, type, normalized, stride,
                           offset);
}

inline auto enable_vertex_attribute(u32 index) -> void {
  rl::rlEnableVertexAttribute(index);
}

inline auto active_texture_slot(i32 slot) -> void {
  rl::rlActiveTextureSlot(slot);
}

inline auto enable_texture_cubemap(u32 cubemap_id) -> void {
  rl::rlEnableTextureCubemap(cubemap_id);
}

inline auto disable_texture_cubemap() -> void { rl::rlDisableTextureCubemap(); }

inline auto load_texture_cubemap(const void *data, i32 size, i32 format,
                                 i32 mipmaps) -> u32 {
  return rl::rlLoadTextureCubemap(data, size, format, mipmaps);
}

} // namespace gl
