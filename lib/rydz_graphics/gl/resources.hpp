#pragma once

#include "rydz_graphics/gl/core.hpp"
#include <string>

namespace gl {

inline Image load_image(const char *path) { return rl::LoadImage(path); }

inline Image load_image(const std::string &path) {
  return load_image(path.c_str());
}

inline void unload_image(Image &image) { image.unload(); }

inline void image_format(Image &image, i32 format) { image.format_to(format); }

inline Image gen_image_with_color(i32 width, i32 height, Color color) {
  return rl::GenImageColor(width, height, color);
}

inline Texture load_texture(const char *path) { return rl::LoadTexture(path); }

inline Texture load_texture(const std::string &path) {
  return load_texture(path.c_str());
}

inline Texture load_texture_from_image(const Image &image) {
  return image.load_texture();
}

inline void unload_texture_id(u32 id) { rl::rlUnloadTexture(id); }

inline Sound load_sound(const char *path) { return rl::LoadSound(path); }

inline Sound load_sound(const std::string &path) {
  return load_sound(path.c_str());
}

inline void unload_sound(Sound &sound) { sound.unload(); }

inline Mesh gen_cube(float width = 1.0F, float height = 1.0F,
                     float length = 1.0F) {
  return rl::GenMeshCube(width, height, length);
}

inline Mesh gen_sphere(float radius = 0.5F, i32 rings = 16, i32 slices = 16) {
  return rl::GenMeshSphere(radius, rings, slices);
}

inline Mesh gen_plane(float width = 10.0F, float length = 10.0F, i32 res_x = 1,
                      i32 res_z = 1) {
  return rl::GenMeshPlane(width, length, res_x, res_z);
}

inline Mesh gen_cylinder(float radius = 0.5F, float height = 1.0F,
                         i32 slices = 16) {
  return rl::GenMeshCylinder(radius, height, slices);
}

inline Mesh gen_torus(float radius = 0.5F, float size = 0.2F,
                      i32 radial_segments = 16, i32 sides = 16) {
  return rl::GenMeshTorus(radius, size, radial_segments, sides);
}

inline Mesh gen_hemisphere(float radius = 0.5f, i32 rings = 16,
                           i32 slices = 16) {
  return rl::GenMeshHemiSphere(radius, rings, slices);
}

inline Mesh gen_knot(float radius = 0.5F, float size = 0.2F,
                     i32 radial_segments = 16, i32 sides = 16) {
  return rl::GenMeshKnot(radius, size, radial_segments, sides);
}

inline void gen_tangents(Mesh &mesh) { mesh.gen_tangents(); }

inline void update_mesh_buffer(Mesh &mesh, i32 index, const void *data,
                               i32 data_size, i32 offset) {
  mesh.update_buffer(index, data, data_size, offset);
}

inline RenderTarget load_render_target(i32 width, i32 height) {
  return rl::LoadRenderTexture(width, height);
}

inline u32 spawn_vertex_array() { return rl::rlLoadVertexArray(); }

inline void enable_vertex_array(u32 vao) { rl::rlEnableVertexArray(vao); }

inline void disable_vertex_array() { rl::rlDisableVertexArray(); }

inline u32 load_vertex_buffer(const void *data, i32 size,
                              bool dynamic = false) {
  return rl::rlLoadVertexBuffer(data, size, dynamic);
}

inline void set_vertex_attribute(u32 index, i32 component_count, i32 type,
                                 bool normalized, i32 stride, i32 offset) {
  rl::rlSetVertexAttribute(index, component_count, type, normalized, stride,
                           offset);
}

inline void enable_vertex_attribute(u32 index) {
  rl::rlEnableVertexAttribute(index);
}

inline void unload_vertex_array(u32 vao) { rl::rlUnloadVertexArray(vao); }

inline void unload_vertex_buffer(u32 vbo) { rl::rlUnloadVertexBuffer(vbo); }

inline void active_texture_slot(i32 slot) { rl::rlActiveTextureSlot(slot); }

inline void enable_texture_cubemap(u32 cubemap_id) {
  rl::rlEnableTextureCubemap(cubemap_id);
}

inline void disable_texture_cubemap() { rl::rlDisableTextureCubemap(); }

inline void draw_vertex_array(i32 offset, i32 count) {
  rl::rlDrawVertexArray(offset, count);
}

inline u32 load_texture_cubemap(const void *data, i32 size, i32 format,
                                i32 mipmaps) {
  return rl::rlLoadTextureCubemap(data, size, format, mipmaps);
}

} // namespace gl
