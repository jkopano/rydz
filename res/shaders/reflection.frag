#version 440 core
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 u_camera_pos;
layout(binding = 5) uniform samplerCube u_environment;
uniform float u_reflectivity;

void main() {
  vec3 I = normalize(FragPos - u_camera_pos);
  vec3 N = normalize(Normal);

  if (dot(I, N) > 0.0) {
    N = -N;
  }

  vec3 R = reflect(I, N);
  vec3 envColor = texture(u_environment, R).rgb;

  if (length(envColor) < 1e-4) {
    envColor = vec3(1.0, 0.0, 1.0);
  }

  FragColor = vec4(envColor, u_reflectivity);
}
