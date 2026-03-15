#version 330
/// BLINN PHONG

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec4 colDiffuse;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform float u_normal_factor;
uniform float u_occlusion_factor;
uniform vec3 u_emissive_factor;

uniform sampler2D texture0;
uniform sampler2D u_roughness_metallic_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;

uniform int u_has_roughness_metallic_texture;
uniform int u_has_normal_texture;
uniform int u_has_occlusion_texture;
uniform int u_has_emissive_texture;

uniform vec4 u_color;
uniform vec3 u_camera_pos;

// --- Light uniforms (individual, matching GlobalLightBuffer::visit_values) ---

// Directional light
uniform vec3 u_dir_light_direction;
uniform float u_dir_light_intensity;
uniform vec3 u_dir_light_color;
uniform int u_has_directional;

// Point lights
uniform int u_point_light_count;

struct PointLight {
  vec3 position;
  float intensity;
  vec3 color;
  float range;
};

uniform vec3  u_point_lights_position[32];
uniform float u_point_lights_intensity[32];
uniform vec3  u_point_lights_color[32];
uniform float u_point_lights_range[32];

// Spot lights
uniform int u_spot_light_count;

struct SpotLight {
  vec3 position;
  float range;
  vec3 direction;
  float intensity;
  vec3 color;
  float inner_cutoff;
  float outer_cutoff;
};

uniform vec3  u_spot_lights_position[32];
uniform float u_spot_lights_range[32];
uniform vec3  u_spot_lights_direction[32];
uniform float u_spot_lights_intensity[32];
uniform vec3  u_spot_lights_color[32];
uniform float u_spot_lights_inner_cutoff[32];
uniform float u_spot_lights_outer_cutoff[32];

vec3 calculateDirectionalLight(
  vec3 lightDir_in,
  float intensity,
  vec3 lightColor,
  vec3 normal,
  vec3 viewDir,
  vec3 baseColor,
  float metallic,
  float roughness
) {
  vec3 lightDir = normalize(-lightDir_in);

  // Diffuse
  float NdotL = max(dot(normal, lightDir), 0.0);
  vec3 diffuse = lightColor * baseColor * NdotL * intensity;

  // Specular
  vec3 halfDir = normalize(lightDir + viewDir);
  float NdotH = max(dot(normal, halfDir), 0.0);
  float shininess = mix(128.0, 32.0, roughness);
  float spec = pow(NdotH, shininess);

  vec3 F0 = mix(vec3(0.04), baseColor, metallic);
  float specStrength = (1.0 - roughness) * 0.3;
  vec3 specular = lightColor * spec * intensity * specStrength * F0 * step(0.01, NdotL);

  return diffuse * (1.0 - metallic) + specular;
}

vec3 calculatePointLight(
  vec3 lightPos,
  float intensity,
  vec3 lightColor,
  float range,
  vec3 fragPos,
  vec3 normal,
  vec3 viewDir,
  vec3 baseColor,
  float metallic,
  float roughness
) {
  vec3 lightDir = lightPos - fragPos;
  float distance = length(lightDir);

  if (distance > range) {
    return vec3(0.0);
  }

  lightDir = normalize(lightDir);

  float attenuation = intensity / (distance * distance + 0.01);
  float factor = distance / range;
  float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
  attenuation *= smoothFactor * smoothFactor;

  // Diffuse
  float NdotL = max(dot(normal, lightDir), 0.0);
  vec3 diffuse = lightColor * baseColor * NdotL * attenuation;

  // Specular
  vec3 halfDir = normalize(lightDir + viewDir);
  float NdotH = max(dot(normal, halfDir), 0.0);
  float shininess = mix(128.0, 32.0, roughness);
  float spec = pow(NdotH, shininess);

  vec3 F0 = mix(vec3(0.04), baseColor, metallic);
  float specStrength = (1.0 - roughness) * 0.3;
  vec3 specular = lightColor * spec * attenuation * specStrength * F0 * step(0.01, NdotL);

  return diffuse * (1.0 - metallic) + specular;
}

vec3 calculateSpotLight(
  vec3 lightPos,
  float range,
  vec3 spotDir,
  float intensity,
  vec3 lightColor,
  float inner_cutoff,
  float outer_cutoff,
  vec3 fragPos,
  vec3 normal,
  vec3 viewDir,
  vec3 baseColor,
  float metallic,
  float roughness
) {
  vec3 lightDir = lightPos - fragPos;
  float distance = length(lightDir);

  if (distance > range) {
    return vec3(0.0);
  }

  lightDir = normalize(lightDir);

  // Spotlight cone
  float theta = dot(lightDir, normalize(-spotDir));
  float epsilon = inner_cutoff - outer_cutoff;
  float spotIntensity = clamp((theta - outer_cutoff) / epsilon, 0.0, 1.0);

  if (spotIntensity <= 0.0) {
    return vec3(0.0);
  }

  float attenuation = intensity / (distance * distance + 0.01);
  float factor = distance / range;
  float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);

  attenuation *= smoothFactor * smoothFactor;
  attenuation *= spotIntensity;

  // Diffuse
  float NdotL = max(dot(normal, lightDir), 0.0);
  vec3 diffuse = lightColor * baseColor * NdotL * attenuation;

  // Specular
  vec3 halfDir = normalize(lightDir + viewDir);
  float NdotH = max(dot(normal, halfDir), 0.0);
  float shininess = mix(128.0, 32.0, roughness);
  float spec = pow(NdotH, shininess);

  vec3 F0 = mix(vec3(0.04), baseColor, metallic);
  float specStrength = (1.0 - roughness) * 0.3;
  vec3 specular = lightColor * spec * attenuation * specStrength * F0 * step(0.01, NdotL);

  return diffuse * (1.0 - metallic) + specular;
}

void main() {
  vec3 normal = normalize(Normal);
  vec3 viewDir = normalize(u_camera_pos - FragPos);

  // Base color
  vec4 baseColor = colDiffuse;
  vec4 diff_tex = texture(texture0, TexCoord);
  if (diff_tex.a < 0.1 && colDiffuse.a > 0.5) {
    discard;
  }
  baseColor *= diff_tex;

  // Metallic | roughness
  float metallic = u_metallic_factor;
  float roughness = u_roughness_factor;
  if (u_has_roughness_metallic_texture != 0) {
    metallic *= texture(u_roughness_metallic_texture, TexCoord).b;
    roughness *= texture(u_roughness_metallic_texture, TexCoord).g;
  }

  // Normal mapping ----- NIE KORZYSTAM FOR NOW
  if (u_has_normal_texture != 0) {
    vec3 n = texture(u_normal_texture, TexCoord).rgb * 2.0 - 1.0;
  }

  // Ambient occlusion
  float ao = u_occlusion_factor;
  if (u_has_occlusion_texture != 0) {
    ao *= texture(u_occlusion_texture, TexCoord).r;
  }
  ao = min(ao, 1.0);

  // Emissive
  vec3 emissive = u_emissive_factor;
  if (u_has_emissive_texture != 0) {
    emissive *= texture(u_emissive_texture, TexCoord).rgb;
  }

  // -------------------
  // Calculate Lighting
  // -------------------

  vec3 lighting = vec3(0.0);

  // Directional light
  if (u_has_directional > 0 && u_dir_light_intensity > 0.0) {
    lighting += calculateDirectionalLight(
        u_dir_light_direction,
        u_dir_light_intensity,
        u_dir_light_color,
        normal,
        viewDir,
        baseColor.rgb,
        metallic,
        roughness
      );
  }

  // Point lights
  for (int i = 0; i < 16; i++) {
    if (i >= u_point_light_count) break;

    if (u_point_lights_intensity[i] <= 0.0) {
      continue;
    }

    lighting += calculatePointLight(
        u_point_lights_position[i],
        u_point_lights_intensity[i],
        u_point_lights_color[i],
        u_point_lights_range[i],
        FragPos,
        normal,
        viewDir,
        baseColor.rgb,
        metallic,
        roughness
      );
  }

  // Spot lights
  for (int i = 0; i < 16; i++) {
    if (i >= u_spot_light_count) break;

    if (u_spot_lights_intensity[i] <= 0.0) {
      continue;
    }

    lighting += calculateSpotLight(
        u_spot_lights_position[i],
        u_spot_lights_range[i],
        u_spot_lights_direction[i],
        u_spot_lights_intensity[i],
        u_spot_lights_color[i],
        u_spot_lights_inner_cutoff[i],
        u_spot_lights_outer_cutoff[i],
        FragPos,
        normal,
        viewDir,
        baseColor.rgb,
        metallic,
        roughness
      );
  }

  vec3 color;

  float ambientStrength = 0.02;
  vec3 ambientColor = baseColor.rgb * ambientStrength;
  color = (ambientColor + lighting) * ao + emissive;

  // color tint
  float strength = u_color.a;
  color = mix(color, color * u_color.rgb, strength);

  // Gamma correction (linear -> sRGB)
  color = pow(color, vec3(1.0 / 2.2));

  FragColor = vec4(color, 1.0);
}
