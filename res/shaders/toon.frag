#version 330

in vec3 FragPos;
in vec3 Normal;

uniform vec4 colDiffuse;
uniform float u_rim_strength;
uniform vec3 lightDir;

out vec4 finalColor;

void main() {
  vec3 norm = normalize(Normal);

  float NdotL = dot(norm, -lightDir);
  float intensity;
  if (NdotL > 0.6) intensity = 1.0;
  else if (NdotL > 0.2) intensity = 0.6;
  else if (NdotL > -0.1) intensity = 0.35;
  else intensity = 0.15;

  vec3 color = colDiffuse.rgb * intensity;

  vec3 viewDir = normalize(-FragPos);
  float rim = 1.0 - max(dot(norm, viewDir), 0.0);
  rim = smoothstep(0.55, 0.7, rim);
  color += vec3(u_rim_strength) * rim;

  finalColor = vec4(color, colDiffuse.a);
}
