#version 440 core
layout(location = 0) in vec3 position;

out vec3 TexCoords;

uniform mat4 u_mat_view;
uniform mat4 u_mat_projection;

void main() {
  TexCoords = position;
  mat4 viewNoTranslation = mat4(mat3(u_mat_view));
  vec4 pos = u_mat_projection * viewNoTranslation * vec4(position, 1.0);
  gl_Position = pos.xyww;
}
