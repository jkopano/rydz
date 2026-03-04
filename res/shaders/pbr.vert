#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
  vec4 world_pos = instanceTransform * vec4(vertexPosition, 1.0);
  FragPos = world_pos.xyz;

  mat3 normalMatrix = mat3(instanceTransform);
  Normal = normalize(normalMatrix * vertexNormal);

  TexCoord = vertexTexCoord;
  gl_Position = mvp * world_pos;
}
