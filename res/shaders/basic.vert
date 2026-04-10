#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;
uniform int u_use_instancing;

out vec2 fragTexCoord;

void main() {
  mat4 model = (u_use_instancing != 0) ? instanceTransform : matModel;
  fragTexCoord = vertexTexCoord;
  gl_Position = mvp * model * vec4(vertexPosition, 1.0);
}
