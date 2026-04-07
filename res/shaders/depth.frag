#version 430

in vec2 TexCoord;

uniform vec4 colDiffuse;
uniform sampler2D texture0;
uniform float u_alpha_cutoff;

void main() {
  vec4 baseColor = colDiffuse * texture(texture0, TexCoord);
  if (baseColor.a < u_alpha_cutoff) {
    discard;
  }
}
