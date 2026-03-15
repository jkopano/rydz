#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
  // Use instanceTransform when instanced, fall back to matModel uniform
  mat4 model = instanceTransform;
  if (model[3][3] == 0.0) model = matModel;

  vec4 world_pos = model * vec4(vertexPosition, 1.0);
  FragPos = world_pos.xyz;

  mat3 normalMatrix = mat3(model);
  Normal = normalize(normalMatrix * vertexNormal);

  TexCoord = vertexTexCoord;
  gl_Position = mvp * world_pos;
}
