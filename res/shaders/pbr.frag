//PBR bez IBL
#version 330

in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec3 Bitangent;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec4 colDiffuse;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform float u_normal_factor;
uniform float u_occlusion_factor;
uniform vec3 u_emissive_factor;

uniform sampler2D texture0;
uniform sampler2D u_metallic_texture;
uniform sampler2D u_roughness_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;

uniform vec4 u_color;
uniform vec3 u_camera_pos;

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

uniform vec3 u_point_lights_position[32];
uniform float u_point_lights_intensity[32];
uniform vec3 u_point_lights_color[32];
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

uniform vec3 u_spot_lights_position[32];
uniform float u_spot_lights_range[32];
uniform vec3 u_spot_lights_direction[32];
uniform float u_spot_lights_intensity[32];
uniform vec3 u_spot_lights_color[32];
uniform float u_spot_lights_inner_cutoff[32];
uniform float u_spot_lights_outer_cutoff[32];

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
      pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float NdotH = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  return a2 / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2 = geometrySchlickGGX(NdotV, roughness);
  float ggx1 = geometrySchlickGGX(NdotL, roughness);
  return ggx1 * ggx2;
}

vec3 evaluatePBR(
  vec3 N,
  vec3 V,
  vec3 L,
  vec3 radiance,
  vec3 albedo,
  float metallic,
  float roughness
) {
  vec3 H = normalize(V + L);
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);
  float HdotV = max(dot(H, V), 0.0);

  if (NdotL <= 0.0 || NdotV <= 0.0) {
    return vec3(0.0);
  }

  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F = fresnelSchlick(HdotV, F0);
  float NDF = distributionGGX(N, H, roughness);
  float G = geometrySmith(N, V, L, roughness);

  vec3 numerator = NDF * G * F;
  float denominator = max(4.0 * NdotV * NdotL, 0.0001);
  vec3 specular = numerator / denominator;

  vec3 kS = F;
  vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
  vec3 diffuse = kD * albedo / PI;

  return (diffuse + specular) * radiance * NdotL;
}

vec3 sampleNormal(vec3 N, vec3 T, vec3 B, vec2 uv) {
  if (dot(T, T) < 0.0001) {
    return N;
  }

  vec3 tangentNormal = texture(u_normal_texture, uv).xyz * 2.0 - 1.0;
  tangentNormal.xy *= u_normal_factor;
  mat3 TBN = mat3(T, B, N);
  return normalize(TBN * tangentNormal);
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
  vec3 albedo = pow(baseColor.rgb, vec3(2.2));

  // Metallic | roughness
  float metallic = clamp(u_metallic_factor, 0.0, 1.0);
  float roughness = clamp(u_roughness_factor, 0.045, 1.0);
  metallic *= texture(u_metallic_texture, TexCoord).r;
  roughness *= texture(u_roughness_texture, TexCoord).r;
  metallic = clamp(metallic, 0.0, 1.0);
  roughness = clamp(roughness, 0.045, 1.0);

  normal = sampleNormal(normalize(normal), normalize(Tangent),
      normalize(Bitangent), TexCoord);

  // Ambient occlusion
  float ao = u_occlusion_factor;
  ao *= texture(u_occlusion_texture, TexCoord).r;
  ao = min(ao, 1.0);

  // Emissive
  vec3 emissive = u_emissive_factor;
  emissive *= pow(texture(u_emissive_texture, TexCoord).rgb, vec3(2.2));

  vec3 lighting = vec3(0.0);

  // Directional light
  if (u_has_directional > 0 && u_dir_light_intensity > 0.0) {
    vec3 lightDir = normalize(-u_dir_light_direction);
    vec3 radiance = u_dir_light_color * u_dir_light_intensity;
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance, albedo, metallic, roughness);
  }

  // Point lights
  for (int i = 0; i < 16; i++) {
    if (i >= u_point_light_count) break;

    if (u_point_lights_intensity[i] <= 0.0) {
      continue;
    }

    vec3 lightVec = u_point_lights_position[i] - FragPos;
    float distance = length(lightVec);
    if (distance > u_point_lights_range[i]) {
      continue;
    }

    vec3 lightDir = normalize(lightVec);
    float attenuation = u_point_lights_intensity[i] / (distance * distance + 0.01);
    float factor = distance / u_point_lights_range[i];
    float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
    attenuation *= smoothFactor * smoothFactor;

    vec3 radiance = u_point_lights_color[i] * attenuation;
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance, albedo, metallic, roughness);
  }

  // Spot lights
  for (int i = 0; i < 16; i++) {
    if (i >= u_spot_light_count) break;

    if (u_spot_lights_intensity[i] <= 0.0) {
      continue;
    }

    vec3 lightVec = u_spot_lights_position[i] - FragPos;
    float distance = length(lightVec);
    if (distance > u_spot_lights_range[i]) {
      continue;
    }

    vec3 lightDir = normalize(lightVec);
    float theta = dot(lightDir, normalize(-u_spot_lights_direction[i]));
    float epsilon = u_spot_lights_inner_cutoff[i] - u_spot_lights_outer_cutoff[i];
    float spotIntensity = clamp((theta - u_spot_lights_outer_cutoff[i]) /
          max(epsilon, 0.0001),
        0.0, 1.0);
    if (spotIntensity <= 0.0) {
      continue;
    }

    float attenuation = u_spot_lights_intensity[i] / (distance * distance + 0.01);
    float factor = distance / u_spot_lights_range[i];
    float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
    attenuation *= smoothFactor * smoothFactor * spotIntensity;

    vec3 radiance = u_spot_lights_color[i] * attenuation;
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance, albedo, metallic, roughness);
  }

  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F = fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, roughness);
  vec3 kS = F;
  vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
  vec3 ambientColor = kD * albedo * 0.03 * ao;
  vec3 color = ambientColor + lighting + emissive;

  float strength = u_color.a;
  color = mix(color, color * u_color.rgb, strength);

  color = color / (color + vec3(1.0));

  color = pow(color, vec3(1.0 / 2.2));

  FragColor = vec4(color, baseColor.a);
}
