#version 430

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;

out vec2 TexCoord;
out vec3 WorldPos;

void main() {
  TexCoord = vertexTexCoord;
  vec4 worldPos = instanceTransform * vec4(vertexPosition, 1.0);
  WorldPos = worldPos.xyz;
  gl_Position = mvp * worldPos;
}
