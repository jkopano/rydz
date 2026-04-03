#version 430

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 u_resolution;
uniform float u_time;
uniform int u_enabled;
uniform float u_exposure;
uniform float u_contrast;
uniform float u_saturation;
uniform float u_vignette;
uniform float u_grain;

out vec4 finalColor;

float hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

vec3 applySaturation(vec3 color, float saturation) {
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  return mix(vec3(luminance), color, saturation);
}

void main() {
  vec4 color = texture(texture0, fragTexCoord);

  if (u_enabled != 0) {
    color.rgb *= u_exposure;
    color.rgb = (color.rgb - 0.5) * u_contrast + 0.5;
    color.rgb = applySaturation(color.rgb, u_saturation);

    vec2 uv = gl_FragCoord.xy / max(u_resolution, vec2(1.0));
    vec2 vignette_uv = uv * (1.0 - uv.yx);
    float vignette = pow(max(vignette_uv.x * vignette_uv.y * 15.0, 0.0), 0.18);
    color.rgb *= mix(1.0 - u_vignette, 1.0, vignette);

    float noise = hash12(gl_FragCoord.xy + vec2(u_time * 60.0));
    color.rgb += (noise - 0.5) * u_grain;
  }

  finalColor = vec4(max(color.rgb, vec3(0.0)), color.a);
}
