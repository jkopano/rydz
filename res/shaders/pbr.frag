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
uniform float u_alpha_cutoff;
uniform int u_render_method;

layout(std140, binding = 0) uniform ViewUniforms {
  vec4 u_camera_pos;
  mat4 u_mat_view;
  mat4 u_mat_projection;
  vec4 u_dir_light_direction;
  vec4 u_dir_light_color_intensity;
  vec4 u_ambient_light_color_intensity;
  ivec4 u_cluster_dimensions;
  vec4 u_cluster_screen_size_near_far;
  ivec4 u_view_flags;
};

layout(std140, binding = 1) uniform ShadowUniforms {
  mat4 u_dir_light_matrices[4];
  vec4 u_cascade_splits;
  vec4 u_cascade_uv_rects[4];
  ivec4 u_shadow_flags;
  vec4 u_shadow_params;
  ivec4 u_point_screen_shadow_flags;
  vec4 u_point_screen_shadow_params;
};

uniform sampler2D u_shadow_atlas;
uniform sampler2D u_scene_depth;
uniform samplerCubeArray u_point_shadow_maps;

const int RENDER_METHOD_OPAQUE = 0;
const int RENDER_METHOD_TRANSPARENT = 1;
const int RENDER_METHOD_ALPHA_CUTOUT = 2;

struct GpuLocalLight {
  vec4 position_range;
  vec4 color_intensity;
  vec4 direction_type;
  vec4 shadow_cone_data;
};

struct ClusterRecord {
  vec4 min_bounds;
  vec4 max_bounds;
  uvec4 meta;
};

layout(std430, binding = 0) readonly buffer LocalLightBuffer {
  GpuLocalLight u_local_lights[];
};

layout(std430, binding = 1) readonly buffer ClusterBuffer {
  ClusterRecord u_clusters[];
};

layout(std430, binding = 2) readonly buffer ClusterIndexBuffer {
  uint u_cluster_light_indices[];
};

const float PI = 3.14159265359;
const float LOCAL_LIGHT_TYPE_POINT = 0.5;
const float LOCAL_LIGHT_TYPE_SPOT = 1.5;

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

float sampleDirectionalDepth(int cascadeIndex, vec2 uv) {
  vec4 rect = u_cascade_uv_rects[cascadeIndex];
  vec2 atlasUv = rect.xy + uv * rect.zw;
  return texture(u_shadow_atlas, atlasUv).r;
}

vec2 directionalTexelSize(int cascadeIndex) {
  vec2 atlasSize = vec2(textureSize(u_shadow_atlas, 0));
  return u_cascade_uv_rects[cascadeIndex].zw / atlasSize;
}

float samplePointShadow(int slot, vec3 dir) {
  return texture(u_point_shadow_maps, vec4(dir, float(slot))).r;
}

float pointShadowTexelScale() {
  return 2.0 / max(float(textureSize(u_point_shadow_maps, 0).x), 1.0);
}

void buildPointShadowBasis(vec3 dir, out vec3 tangent, out vec3 bitangent) {
  vec3 up = abs(dir.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
  tangent = normalize(cross(up, dir));
  bitangent = cross(dir, tangent);
}

float computePointShadowBias(float currentDepth, float farPlane, vec3 normal, vec3 lightDir) {
  float baseBias = u_shadow_params.z;
  float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
  float angleFactor = mix(2.0, 1.0, cosTheta);
  float distanceScale = mix(1.0, 2.0, clamp(currentDepth / farPlane, 0.0, 1.0));

  return (baseBias * angleFactor * distanceScale) / farPlane;
}

float samplePointShadowPCF(
  int slot,
  vec3 lightToFrag,
  float currentDepth,
  float farPlane,
  vec3 normal,
  vec3 lightDir
) {
  if (currentDepth <= 0.001) return 1.0;

  // Miękkie wygaszanie cienia przy granicy zasięgu światła (farPlane)
  float shadowFade = smoothstep(farPlane, farPlane * 0.9, currentDepth);
  if (shadowFade <= 0.0) return 1.0;

  int pcfRadius = max(u_point_screen_shadow_flags.z, 0);
  float currentDepthNormalized = currentDepth / farPlane;
  float bias = computePointShadowBias(currentDepth, farPlane, normal, lightDir);

  if (pcfRadius <= 0) {
    float closestDepth = samplePointShadow(slot, lightToFrag);
    float s = currentDepthNormalized - bias > closestDepth ? 0.0 : 1.0;
    return mix(1.0, s, shadowFade);
  }

  vec3 dir = normalize(lightToFrag);
  vec3 tangent, bitangent;
  buildPointShadowBasis(dir, tangent, bitangent);

  float texelScale = pointShadowTexelScale();
  float visibility = 0.0;
  float sampleCount = 0.0;

  for (int x = -pcfRadius; x <= pcfRadius; ++x) {
    for (int y = -pcfRadius; y <= pcfRadius; ++y) {
      vec2 offset = vec2(float(x), float(y)) * texelScale;
      vec3 sampleDir = normalize(dir + tangent * offset.x + bitangent * offset.y);
      float closestDepth = samplePointShadow(slot, sampleDir);
      visibility += currentDepthNormalized - bias > closestDepth ? 0.0 : 1.0;
      sampleCount += 1.0;
    }
  }

  float finalShadow = visibility / max(sampleCount, 1.0);
  return mix(1.0, finalShadow, shadowFade);
}

float sceneDepthToViewDepth(float depthSample) {
  float nearPlane = max(u_cluster_screen_size_near_far.z, 0.001);
  float farPlane = max(u_cluster_screen_size_near_far.w, nearPlane + 0.001);

  if (u_view_flags.z > 0) {
    return mix(nearPlane, farPlane, clamp(depthSample, 0.0, 1.0));
  }

  float z = depthSample * 2.0 - 1.0;
  float denom = farPlane + nearPlane - z * (farPlane - nearPlane);
  return (2.0 * nearPlane * farPlane) / max(denom, 0.0001);
}

vec2 projectViewToUv(vec3 viewPos, out float ndcDepth) {
  vec4 clip = u_mat_projection * vec4(viewPos, 1.0);
  if (clip.w <= 0.0001) {
    ndcDepth = 2.0;
    return vec2(-1.0);
  }

  vec3 ndc = clip.xyz / clip.w;
  ndcDepth = ndc.z;
  return ndc.xy * 0.5 + 0.5;
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

float computeDirectionalShadow(vec3 fragPos, vec3 normal, vec3 lightDir, vec3 viewPos) {
  if (u_shadow_flags.x <= 0 || u_shadow_flags.y <= 0) {
    return 1.0;
  }

  float depth = max(-viewPos.z, max(u_cluster_screen_size_near_far.z, 0.001));
  int cascadeIndex = 0;
  int cascadeCount = clamp(u_shadow_flags.y, 1, 4);
  for (int i = 0; i < cascadeCount - 1; ++i) {
    if (depth > u_cascade_splits[i]) {
      cascadeIndex = i + 1;
    }
  }

  vec4 shadowClip = u_dir_light_matrices[cascadeIndex] * vec4(fragPos, 1.0);
  vec3 shadowCoord = shadowClip.xyz / max(shadowClip.w, 0.0001);
  shadowCoord = shadowCoord * 0.5 + 0.5;

  if (shadowCoord.z > 1.0 || shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
      shadowCoord.y < 0.0 || shadowCoord.y > 1.0) {
    return 1.0;
  }

  float bias = max(
      u_shadow_params.x,
      u_shadow_params.y * (1.0 - max(dot(normal, lightDir), 0.0))
    );
  vec2 texelSize = directionalTexelSize(cascadeIndex);
  int pcfRadius = max(u_shadow_flags.w, 0);
  float visibility = 0.0;
  float sampleCount = 0.0;

  for (int x = -pcfRadius; x <= pcfRadius; ++x) {
    for (int y = -pcfRadius; y <= pcfRadius; ++y) {
      vec2 offset = vec2(float(x), float(y)) * texelSize;
      float closestDepth = sampleDirectionalDepth(cascadeIndex, shadowCoord.xy + offset);
      visibility += shadowCoord.z - bias > closestDepth ? 0.0 : 1.0;
      sampleCount += 1.0;
    }
  }

  return visibility / max(sampleCount, 1.0);
}

bool isPointLight(GpuLocalLight light) {
  return light.direction_type.w < LOCAL_LIGHT_TYPE_POINT;
}

bool isSpotLight(GpuLocalLight light) {
  return light.direction_type.w >= LOCAL_LIGHT_TYPE_POINT &&
    light.direction_type.w < LOCAL_LIGHT_TYPE_SPOT;
}

float computeSpotAttenuation(GpuLocalLight light, vec3 lightDir) {
  vec3 spotDirection = normalize(light.direction_type.xyz);
  float innerCos = light.shadow_cone_data.z;
  float outerCos = light.shadow_cone_data.w;
  float cosTheta = dot(-lightDir, spotDirection);
  float cone = clamp(
      (cosTheta - outerCos) / max(innerCos - outerCos, 0.0001),
      0.0,
      1.0
    );
  return cone * cone;
}

float computePointScreenSpaceShadow(vec3 viewPos, GpuLocalLight pointLight) {
  if (u_point_screen_shadow_flags.x <= 0) return 1.0;

  vec3 lightViewPos = (u_mat_view * vec4(pointLight.position_range.xyz, 1.0)).xyz;
  vec3 rayDirFull = lightViewPos - viewPos;
  float rayLength = length(rayDirFull);
  vec3 rayDir = rayDirFull / rayLength;

  int steps = max(u_point_screen_shadow_flags.y, 1);
  float thickness = max(u_point_screen_shadow_params.x, 0.2);

  // DITHERING: Dodajemy małe losowe przesunięcie startu promienia.
  // To zamienia "schodki" (banding) w drobne ziarno.
  float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
  float startDistance = 0.15 + (dither * 0.1);

  float stepLength = (rayLength - startDistance) / float(steps);
  float maxOcclusion = 0.0;
  vec2 finalUV = vec2(0.0);

  for (int step = 0; step < steps; ++step) {
    // Przesuwamy każdy krok o dither, żeby próbki nie układały się w linie
    float travel = startDistance + stepLength * (float(step) + dither);
    vec3 samplePos = viewPos + rayDir * travel;

    float ndcDepth = 0.0;
    vec2 uv = projectViewToUv(samplePos, ndcDepth);
    finalUV = uv;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) continue;

    float sceneDepthSample = texture(u_scene_depth, uv).r;
    if (sceneDepthSample >= 0.9999) continue;

    float sceneDepth = sceneDepthToViewDepth(sceneDepthSample);
    float rayDepth = -samplePos.z;
    float diff = rayDepth - sceneDepth;

    // Kluczowe: Warunek kolizji z "miękkim marginesem"
    if (diff > 0.0 && diff < thickness) {
      // Im cieńszy obiekt (mały diff), tym mocniejszy cień.
      // Im dalej od punktu startu, tym bardziej rozmywamy (travel / rayLength).
      float softness = clamp(1.0 - (diff / thickness), 0.0, 1.0);
      float distFade = clamp(1.0 - (travel / rayLength), 0.0, 1.0);

      // Rejestrujemy occlusion
      maxOcclusion = max(maxOcclusion, softness * distFade);

      // Jeśli mamy mocną kolizję, wychodzimy szybciej
      if (maxOcclusion > 0.9) break;
    }
  }

  // Płynne wygaszanie na krawędziach ekranu (żeby cienie nie migały przy krawędziach)
  float edgeFade = 1.0 - max(
        smoothstep(0.8, 1.0, abs(finalUV.x * 2.0 - 1.0)),
        smoothstep(0.8, 1.0, abs(finalUV.y * 2.0 - 1.0))
      );

  // Mix finalny: 1.0 to brak cienia, 0.0 to pełny cień
  float shadow = mix(1.0, 1.0 - maxOcclusion, edgeFade);

  // Zapobiegamy całkowitej czerni SSS (często wygląda to nienaturalnie)
  return clamp(shadow, 0.1, 1.0);
}

float computePointShadow(vec3 fragPos, vec3 viewPos, vec3 normal, GpuLocalLight pointLight) {
  if (pointLight.shadow_cone_data.y > 0.5) {
    return computePointScreenSpaceShadow(viewPos, pointLight);
  }

  if (pointLight.shadow_cone_data.x < 0.0) return 1.0;

  int shadowSlot = int(pointLight.shadow_cone_data.x);
  if (shadowSlot < 0 || shadowSlot >= u_shadow_flags.z) return 1.0;

  vec3 lightVec = pointLight.position_range.xyz - fragPos;
  vec3 lightToFrag = -lightVec;
  float currentDepth = length(lightToFrag);
  float farPlane = max(pointLight.position_range.w, 0.001);

  // Przekazujemy normalną i kierunek światła do PCF (dla biasu)
  return samplePointShadowPCF(shadowSlot, lightToFrag, currentDepth, farPlane, normal, normalize(lightVec));
}

int computeDepthSlice(float depth, ivec3 dims) {
  float nearPlane = max(u_cluster_screen_size_near_far.z, 0.001);
  float farPlane = max(u_cluster_screen_size_near_far.w, nearPlane + 0.001);
  float normalizedDepth = 0.0;

  if (u_view_flags.z > 0) {
    normalizedDepth = (depth - nearPlane) / max(farPlane - nearPlane, 0.001);
  } else {
    normalizedDepth = log(depth / nearPlane) / log(farPlane / nearPlane);
  }

  normalizedDepth = clamp(normalizedDepth, 0.0, 0.999999);
  return min(int(normalizedDepth * float(dims.z)), dims.z - 1);
}

int computeClusterIndex(vec3 viewPos) {
  ivec3 dims = max(u_cluster_dimensions.xyz, ivec3(1));
  vec2 tileSize =
    u_cluster_screen_size_near_far.xy / vec2(float(dims.x), float(dims.y));
  tileSize = max(tileSize, vec2(1.0));

  ivec2 tile = ivec2(gl_FragCoord.xy / tileSize);
  tile = clamp(tile, ivec2(0), dims.xy - ivec2(1));

  float depth = max(-viewPos.z, max(u_cluster_screen_size_near_far.z, 0.001));
  int slice = computeDepthSlice(depth, dims);
  return (slice * dims.y + tile.y) * dims.x + tile.x;
}

void main() {
  vec3 normal = normalize(Normal);
  vec3 viewDir = normalize(u_camera_pos.xyz - FragPos);

  vec4 baseColor = colDiffuse;
  vec4 diffTex = texture(texture0, TexCoord);
  baseColor *= diffTex;
  if (u_render_method != RENDER_METHOD_OPAQUE && baseColor.a < u_alpha_cutoff) {
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
  vec3 viewPos = (u_mat_view * vec4(FragPos, 1.0)).xyz;

  if (u_view_flags.x > 0 && u_dir_light_color_intensity.w > 0.0) {
    vec3 lightDir = normalize(-u_dir_light_direction.xyz);
    vec3 radiance =
      u_dir_light_color_intensity.xyz * u_dir_light_color_intensity.w;
    float shadow = computeDirectionalShadow(FragPos, normal, lightDir, viewPos);
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance * shadow, albedo, metallic, roughness);
  }

  int clusterIndex = computeClusterIndex(viewPos);
  ClusterRecord cluster = u_clusters[clusterIndex];
  uint localLightCount = min(cluster.meta.y, uint(max(u_view_flags.y, 0)));

  for (uint i = 0u; i < localLightCount; ++i) {
    uint lightIndex = u_cluster_light_indices[cluster.meta.x + i];
    GpuLocalLight localLight = u_local_lights[lightIndex];

    if (localLight.color_intensity.w <= 0.0) {
      continue;
    }

    vec3 lightVec = localLight.position_range.xyz - FragPos;
    float distance = length(lightVec);
    if (distance <= 0.001) {
      continue;
    }

    vec3 lightDir = normalize(lightVec);
    float attenuation = localLight.color_intensity.w / (distance * distance + 0.01);
    float factor = distance / localLight.position_range.w;

    // Debug: visualize distance factor
    // if (factor > 0.9 && factor < 1.1) {
    //   lighting += vec3(10.0, 0.0, 0.0);
    // }

    // Smooth falloff without hard cutoff at range boundary
    // Use a function that approaches 0 asymptotically instead of hitting 0 at factor=1
    float smoothFactor = 1.0 / (1.0 + factor * factor * factor * factor);
    attenuation *= smoothFactor;

    if (isSpotLight(localLight)) {
      float coneAttenuation = computeSpotAttenuation(localLight, lightDir);
      if (coneAttenuation <= 0.0) {
        continue;
      }
      attenuation *= coneAttenuation;
    }

    vec3 radiance = localLight.color_intensity.rgb * attenuation;
    float shadow = isPointLight(localLight)
      ? computePointShadow(FragPos, viewPos, normal, localLight) : 1.0;
    lighting += evaluatePBR(
        normal, viewDir, lightDir, radiance * shadow, albedo, metallic, roughness);
  }

  vec3 F0 = mix(vec3(0.04), albedo, metallic);
  vec3 F = fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, roughness);
  vec3 kS = F;
  vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
  vec3 ambientColor =
    kD * albedo * u_ambient_light_color_intensity.rgb *
      u_ambient_light_color_intensity.w * ao;
  vec3 color = ambientColor + lighting + emissive;

  float strength = u_color.a;
  color = mix(color, color * u_color.rgb, strength);
  color = color / (color + vec3(1.0));
  color = pow(color, vec3(1.0 / 2.2));

  // vec2 screenUV = gl_FragCoord.xy / u_cluster_screen_size_near_far.xy;
  // float rawDepth = texture(u_scene_depth, screenUV).r;
  // float linearDepth = sceneDepthToViewDepth(rawDepth);
  // float near = u_cluster_screen_size_near_far.z;
  // float far = u_cluster_screen_size_near_far.w;
  // float visualDepth = (linearDepth - near) / (far - near);
  // color = vec3(clamp(visualDepth, 0.0, 1.0));

  // Debug: visualize light count per cluster
  // float lightCountVis = float(localLightCount) / 8.0;
  // color = mix(color, vec3(0.0, 1.0, 0.0), lightCountVis);

  // Debug: visualize total lighting contribution
  // float lightingVis = length(lighting) / 10.0;
  // color = vec3(lightingVis);

  // Debug: visualize shadows
  // Uncomment to see shadow values (red = shadow, green = no shadow)
  // vec3 shadowDebug = vec3(0.0);
  // bool hasLights = false;
  //
  // for (uint i = 0u; i < localLightCount; ++i) {
  //   uint lightIndex = u_cluster_light_indices[cluster.meta.x + i];
  //   GpuLocalLight localLight = u_local_lights[lightIndex];
  //
  //   if (localLight.color_intensity.w > 0.0) {
  //     float s = computePointShadow(FragPos, viewPos, normal, localLight);
  //     hasLights = true;
  //
  //     if (localLight.shadow_cone_data.y > 0.5) {
  //       shadowDebug.r += (1.0 - s);
  //     } else {
  //       shadowDebug.g += (1.0 - s);
  //     }
  //   }
  // }
  //
  // if (hasLights) {
  //   color = shadowDebug;
  //   color *= 1.0;
  // } else {
  //   color = vec3(0.0, 0.0, 0.2);
  // }

  FragColor = vec4(color, 1.0);
}
