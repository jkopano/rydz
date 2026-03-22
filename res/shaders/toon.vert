#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;

out vec3 FragPos;
out vec3 Normal;

void main() {
  mat4 model = instanceTransform;
  if (model[3][3] == 0.0) model = matModel;

  vec4 world_pos = model * vec4(vertexPosition, 1.0);
  FragPos = world_pos.xyz;
  Normal = normalize(mat3(model) * vertexNormal);
  gl_Position = mvp * world_pos;
}
