#version 430

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;

out vec2 TexCoord;

void main() {
  mat4 model = instanceTransform;
  if (model[3][3] == 0.0) {
    model = matModel;
  }

  TexCoord = vertexTexCoord;
  gl_Position = mvp * model * vec4(vertexPosition, 1.0);
}
