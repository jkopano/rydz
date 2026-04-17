#version 430

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
uniform sampler2D texture1;
uniform sampler2D u_roughness_texture;
uniform sampler2D texture2;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;

uniform vec4 u_color;
uniform vec3 cameraPos;
uniform mat4 matView;

uniform vec3 u_dir_light_direction;
uniform float u_dir_light_intensity;
uniform vec3 u_dir_light_color;
uniform int u_has_directional;

uniform ivec4 u_cluster_dimensions;
uniform vec2 u_cluster_screen_size;
uniform vec2 u_cluster_near_far;
uniform int u_cluster_max_lights;
uniform int u_is_orthographic;
uniform float alphaCutoff;
uniform int u_render_method;

const int RENDER_METHOD_OPAQUE = 0;
const int RENDER_METHOD_TRANSPARENT = 1;
const int RENDER_METHOD_ALPHA_CUTOUT = 2;

struct GpuPointLight {
  vec4 position_range;
  vec4 color_intensity;
};

struct ClusterRecord {
  vec4 min_bounds;
  vec4 max_bounds;
  uvec4 meta;
};

layout(std430, binding = 0) readonly buffer PointLightBuffer {
  GpuPointLight u_point_lights[];
};

layout(std430, binding = 1) readonly buffer ClusterBuffer {
  ClusterRecord u_clusters[];
};

layout(std430, binding = 2) readonly buffer ClusterIndexBuffer {
  uint u_cluster_light_indices[];
};

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

  vec3 tangentNormal = texture(texture2, uv).xyz * 2.0 - 1.0;
  tangentNormal.xy *= u_normal_factor;
  mat3 TBN = mat3(T, B, N);
  return normalize(TBN * tangentNormal);
}

int computeDepthSlice(float depth, ivec3 dims) {
  float nearPlane = max(u_cluster_near_far.x, 0.001);
  float farPlane = max(u_cluster_near_far.y, nearPlane + 0.001);
  float normalizedDepth = 0.0;

  if (u_is_orthographic > 0) {
    normalizedDepth = (depth - nearPlane) / max(farPlane - nearPlane, 0.001);
  } else {
    normalizedDepth = log(depth / nearPlane) / log(farPlane / nearPlane);
  }

  normalizedDepth = clamp(normalizedDepth, 0.0, 0.999999);
  return min(int(normalizedDepth * float(dims.z)), dims.z - 1);
}

int computeClusterIndex(vec3 viewPos) {
  ivec3 dims = max(u_cluster_dimensions.xyz, ivec3(1));
  vec2 tileSize = u_cluster_screen_size / vec2(float(dims.x), float(dims.y));
  tileSize = max(tileSize, vec2(1.0));

  ivec2 tile = ivec2(gl_FragCoord.xy / tileSize);
  tile = clamp(tile, ivec2(0), dims.xy - ivec2(1));

  float depth = max(-viewPos.z, max(u_cluster_near_far.x, 0.001));
  int slice = computeDepthSlice(depth, dims);
  return (slice * dims.y + tile.y) * dims.x + tile.x;
}

void main() {
  vec3 normal = normalize(Normal);
  vec3 viewDir = normalize(cameraPos - FragPos);

  vec4 baseColor = colDiffuse;
  vec4 diffTex = texture(texture0, TexCoord);
  baseColor *= diffTex;
  if (u_render_method != RENDER_METHOD_OPAQUE && baseColor.a < alphaCutoff) {
    discard;
  }
  float outputAlpha = baseColor.a;
  if (u_render_method != RENDER_METHOD_TRANSPARENT) {
    outputAlpha = 1.0;
  }
  vec3 albedo = pow(baseColor.rgb, vec3(2.2));

  float metallic = clamp(u_metallic_factor, 0.0, 1.0);
  float roughness = clamp(u_roughness_factor, 0.045, 1.0);
  metallic *= texture(texture1, TexCoord).r;
  roughness *= texture(u_roughness_texture, TexCoord).r;
  metallic = clamp(metallic, 0.0, 1.0);
  roughness = clamp(roughness, 0.045, 1.0);

  normal = sampleNormal(normalize(normal), normalize(Tangent),
      normalize(Bitangent), TexCoord);

  float ao = u_occlusion_factor;
  ao *= texture(u_occlusion_texture, TexCoord).r;
  ao = min(ao, 1.0);

  vec3 emissive = u_emissive_factor;
  emissive *= pow(texture(u_emissive_texture, TexCoord).rgb, vec3(2.2));

  vec3 lighting = vec3(0.0);

  if (u_has_directional > 0 && u_dir_light_intensity > 0.0) {
    vec3 lightDir = normalize(-u_dir_light_direction);
    vec3 radiance = u_dir_light_color * u_dir_light_intensity;
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance, albedo, metallic, roughness);
  }

  vec3 viewPos = (matView * vec4(FragPos, 1.0)).xyz;
  int clusterIndex = computeClusterIndex(viewPos);
  ClusterRecord cluster = u_clusters[clusterIndex];
  uint pointLightCount = min(cluster.meta.y, uint(max(u_cluster_max_lights, 0)));

  for (uint i = 0u; i < pointLightCount; ++i) {
    uint lightIndex = u_cluster_light_indices[cluster.meta.x + i];
    GpuPointLight pointLight = u_point_lights[lightIndex];

    if (pointLight.color_intensity.w <= 0.0) {
      continue;
    }

    vec3 lightVec = pointLight.position_range.xyz - FragPos;
    float distance = length(lightVec);
    if (distance > pointLight.position_range.w) {
      continue;
    }

    vec3 lightDir = normalize(lightVec);
    float attenuation = pointLight.color_intensity.w / (distance * distance + 0.01);
    float factor = distance / pointLight.position_range.w;
    float smoothFactor = clamp(1.0 - factor * factor * factor * factor, 0.0, 1.0);
    attenuation *= smoothFactor * smoothFactor;

    vec3 radiance = pointLight.color_intensity.rgb * attenuation;
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

  FragColor = vec4(color, outputAlpha);
}
