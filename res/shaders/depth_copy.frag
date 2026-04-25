#version 430

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_depth_texture;

void main() {
  float depth = texture(u_depth_texture, TexCoord).r;

  if (depth > 0.5) {
    FragColor = vec4(1.0, 0.0, 1.0, 1.0);
  } else {
    FragColor = vec4(0.0, 1.0, 1.0, 1.0);
  }
}
