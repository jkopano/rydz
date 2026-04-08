#version 440 core
layout(location = 0) in vec3 position;

out vec3 TexCoords;

uniform mat4 matView;
uniform mat4 matProjection;

void main() {
  TexCoords = position;
  mat4 viewNoTranslation = mat4(mat3(matView));
  vec4 pos = matProjection * viewNoTranslation * vec4(position, 1.0);
  gl_Position = pos.xyww;
}
