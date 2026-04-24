#version 430

in vec2 TexCoord;
in vec3 WorldPos;

uniform vec4 colDiffuse;
uniform sampler2D texture0;
uniform float u_alpha_cutoff;
uniform int u_render_method;
uniform vec3 u_light_pos;
uniform float u_far_plane;

const int RENDER_METHOD_ALPHA_CUTOUT = 2;

void main() {
  vec4 baseColor = colDiffuse * texture(texture0, TexCoord);
  if (u_render_method == RENDER_METHOD_ALPHA_CUTOUT &&
      baseColor.a < u_alpha_cutoff) {
    discard;
  }

  float lightDistance = length(WorldPos - u_light_pos);
  gl_FragDepth = clamp(lightDistance / max(u_far_plane, 0.001), 0.0, 1.0);
}
