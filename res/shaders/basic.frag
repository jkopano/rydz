#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float u_alpha_cutoff;
uniform int u_render_method;

const int RENDER_METHOD_OPAQUE = 0;
const int RENDER_METHOD_TRANSPARENT = 1;

out vec4 finalColor;

void main() {
  vec4 texelColor = texture(texture0, fragTexCoord);
  finalColor = texelColor * colDiffuse;
  if (u_render_method != RENDER_METHOD_OPAQUE && finalColor.a < u_alpha_cutoff) {
    discard;
  }
  if (u_render_method != RENDER_METHOD_TRANSPARENT) {
    finalColor.a = 1.0;
  }
}
