#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;

out vec2 fragTexCoord;

void main() {
  mat4 model = instanceTransform;
  if (model[3][3] == 0.0) model = matModel;
  fragTexCoord = vertexTexCoord;
  gl_Position = mvp * model * vec4(vertexPosition, 1.0);
}
