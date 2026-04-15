#version 430

in vec2 TexCoord;

uniform vec4 colDiffuse;
uniform sampler2D texture0;
uniform float u_alpha_cutoff;
uniform int u_render_method;

const int RENDER_METHOD_ALPHA_CUTOUT = 2;

void main() {
  vec4 baseColor = colDiffuse * texture(texture0, TexCoord);
  if (u_render_method == RENDER_METHOD_ALPHA_CUTOUT &&
      baseColor.a < u_alpha_cutoff) {
    discard;
  }
}
