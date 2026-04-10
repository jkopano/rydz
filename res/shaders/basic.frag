#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float u_alpha_cutoff;

out vec4 finalColor;

void main() {
  vec4 texelColor = texture(texture0, fragTexCoord);
  finalColor = texelColor * colDiffuse;
  if (finalColor.a < u_alpha_cutoff) {
    discard;
  }
}
