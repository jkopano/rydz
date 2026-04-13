#pragma once

#include "rydz_graphics/gl/core.hpp"
#include <string>

namespace gl {

inline Image load_image(const char *path) { return rl::LoadImage(path); }

inline Image load_image(const std::string &path) {
  return load_image(path.c_str());
}

inline void unload_image(Image &image) { image.unload(); }

inline void image_format(Image &image, int format) { image.format_to(format); }

inline Image gen_image_color(int width, int height, Color color) {
  return rl::GenImageColor(width, height, color);
}

inline Texture load_texture(const char *path) { return rl::LoadTexture(path); }

inline Texture load_texture(const std::string &path) {
  return load_texture(path.c_str());
}

inline Texture load_texture_from_image(const Image &image) {
  return image.load_texture();
}

inline void unload_texture(Texture &texture) { texture.unload(); }

inline void unload_texture_id(unsigned int id) { rl::rlUnloadTexture(id); }

inline void set_texture_filter(Texture texture, int filter) {
  texture.set_filter(filter);
}

inline Sound load_sound(const char *path) { return rl::LoadSound(path); }

inline Sound load_sound(const std::string &path) {
  return load_sound(path.c_str());
}

inline void unload_sound(Sound &sound) { sound.unload(); }

inline Mesh gen_cube(float width = 1.0f, float height = 1.0f,
                     float length = 1.0f) {
  return rl::GenMeshCube(width, height, length);
}

inline Mesh gen_sphere(float radius = 0.5f, int rings = 16, int slices = 16) {
  return rl::GenMeshSphere(radius, rings, slices);
}

inline Mesh gen_plane(float width = 10.0f, float length = 10.0f, int res_x = 1,
                      int res_z = 1) {
  return rl::GenMeshPlane(width, length, res_x, res_z);
}

inline Mesh gen_cylinder(float radius = 0.5f, float height = 1.0f,
                         int slices = 16) {
  return rl::GenMeshCylinder(radius, height, slices);
}

inline Mesh gen_torus(float radius = 0.5f, float size = 0.2f,
                      int radial_segments = 16, int sides = 16) {
  return rl::GenMeshTorus(radius, size, radial_segments, sides);
}

inline Mesh gen_hemisphere(float radius = 0.5f, int rings = 16,
                           int slices = 16) {
  return rl::GenMeshHemiSphere(radius, rings, slices);
}

inline Mesh gen_knot(float radius = 0.5f, float size = 0.2f,
                     int radial_segments = 16, int sides = 16) {
  return rl::GenMeshKnot(radius, size, radial_segments, sides);
}

inline void gen_tangents(Mesh &mesh) { mesh.gen_tangents(); }

inline void upload_mesh(Mesh &mesh, bool dynamic = false) {
  mesh.upload(dynamic);
}

inline void unload_mesh(Mesh &mesh) { mesh.unload(); }

inline bool mesh_uploaded(const Mesh &mesh) { return mesh.uploaded(); }

inline int mesh_vertex_count(const Mesh &mesh) { return mesh.vertex_count(); }

inline const float *mesh_vertices(const Mesh &mesh) {
  return mesh.vertex_data();
}

inline float *mesh_vertices(Mesh &mesh) { return mesh.vertex_data(); }

inline void update_mesh_buffer(Mesh &mesh, int index, const void *data,
                               int data_size, int offset) {
  mesh.update_buffer(index, data, data_size, offset);
}

inline RenderTarget load_render_target(int width, int height) {
  return rl::LoadRenderTexture(width, height);
}

inline void unload_render_target(RenderTarget &target) { target.unload(); }

inline Texture &render_target_texture(RenderTarget &target) {
  return target.texture;
}

inline const Texture &render_target_texture(const RenderTarget &target) {
  return target.texture;
}

inline bool render_target_ready(const RenderTarget &target) {
  return target.ready();
}

inline Model load_model(const char *path) { return rl::LoadModel(path); }

inline Model load_model(const std::string &path) {
  return load_model(path.c_str());
}

inline Model load_model_from_mesh(const Mesh &mesh) {
  return rl::LoadModelFromMesh(mesh);
}

inline void unload_model(Model &model) { model.unload(); }

inline unsigned int load_vertex_array() { return rl::rlLoadVertexArray(); }

inline void enable_vertex_array(unsigned int vao) {
  rl::rlEnableVertexArray(vao);
}

inline void disable_vertex_array() { rl::rlDisableVertexArray(); }

inline unsigned int load_vertex_buffer(const void *data, int size,
                                       bool dynamic = false) {
  return rl::rlLoadVertexBuffer(data, size, dynamic);
}

inline void set_vertex_attribute(unsigned int index, int component_count,
                                 int type, bool normalized, int stride,
                                 int offset) {
  rl::rlSetVertexAttribute(index, component_count, type, normalized, stride,
                           offset);
}

inline void enable_vertex_attribute(unsigned int index) {
  rl::rlEnableVertexAttribute(index);
}

inline void unload_vertex_array(unsigned int vao) {
  rl::rlUnloadVertexArray(vao);
}

inline void unload_vertex_buffer(unsigned int vbo) {
  rl::rlUnloadVertexBuffer(vbo);
}

inline void active_texture_slot(int slot) { rl::rlActiveTextureSlot(slot); }

inline void enable_texture_cubemap(unsigned int cubemap_id) {
  rl::rlEnableTextureCubemap(cubemap_id);
}

inline void disable_texture_cubemap() { rl::rlDisableTextureCubemap(); }

inline void draw_vertex_array(int offset, int count) {
  rl::rlDrawVertexArray(offset, count);
}

inline unsigned int load_texture_cubemap(const void *data, int size, int format,
                                         int mipmaps) {
  return rl::rlLoadTextureCubemap(data, size, format, mipmaps);
}

} // namespace gl
