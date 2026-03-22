#version 330

in vec3 FragPos;
in vec3 Normal;

uniform vec4 colDiffuse;

out vec4 finalColor;

void main() {
  vec3 lightDir = normalize(vec3(-0.3, -1.0, -0.5));
  vec3 norm = normalize(Normal);

  // cel-shading: kwantyzacja oświetlenia do 3 poziomów
  float NdotL = dot(norm, -lightDir);
  float intensity;
  if (NdotL > 0.6)       intensity = 1.0;
  else if (NdotL > 0.2)  intensity = 0.6;
  else if (NdotL > -0.1) intensity = 0.35;
  else                    intensity = 0.15;

  vec3 color = colDiffuse.rgb * intensity;

  // rim light (kontur)
  vec3 viewDir = normalize(-FragPos);
  float rim = 1.0 - max(dot(norm, viewDir), 0.0);
  rim = smoothstep(0.55, 0.7, rim);
  color += vec3(0.15) * rim;

  finalColor = vec4(color, colDiffuse.a);
}
