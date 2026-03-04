#version 440 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 u_camera_pos;
layout(binding = 5) uniform samplerCube u_environment;
uniform vec3 u_ior;
uniform float u_transparency;
uniform vec4 u_tint_color;

void main() {
  vec3 I = normalize(FragPos - u_camera_pos);
  vec3 N = normalize(Normal);

  // Flip normal if we are looking at the back face (for double-sided refraction)
  if (dot(I, N) > 0.0) {
    N = -N;
  }

  vec3 R_red = refract(I, N, 1.00 / u_ior.r);
  vec3 R_green = refract(I, N, 1.00 / u_ior.g);
  vec3 R_blue = refract(I, N, 1.00 / u_ior.b);

  if (length(R_red) == 0.0) R_red = reflect(I, N);
  if (length(R_green) == 0.0) R_green = reflect(I, N);
  if (length(R_blue) == 0.0) R_blue = reflect(I, N);

  float r = texture(u_environment, R_red).r;
  float g = texture(u_environment, R_green).g;
  float b = texture(u_environment, R_blue).b;

  vec3 refractionColor = vec3(r, g, b);
  vec3 finalColor = mix(refractionColor, u_tint_color.rgb, u_tint_color.a * 0.3);

  // Debug fallback: show cyan if the sample is essentially black
  if (length(finalColor) < 1e-4) {
    finalColor = vec3(0.0, 1.0, 1.0);
  }

  FragColor = vec4(finalColor, u_transparency);
}
