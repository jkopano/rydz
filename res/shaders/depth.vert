#version 430

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;

out vec2 TexCoord;

void main() {
  TexCoord = vertexTexCoord;
  gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
