#version 430

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexTangent;
in mat4 instanceTransform;

uniform mat4 mvp;
uniform mat4 matModel;

out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 Bitangent;
out vec2 TexCoord;

void main() {
  mat4 model = instanceTransform;
  if (model[3][3] == 0.0) {
    model = matModel;
  }

  vec4 world_pos = model * vec4(vertexPosition, 1.0);
  FragPos = world_pos.xyz;

  mat3 normalMatrix = transpose(inverse(mat3(model)));
  Normal = normalize(normalMatrix * vertexNormal);

  vec3 worldTangent = mat3(model) * vertexTangent.xyz;
  Tangent = normalize(worldTangent - dot(worldTangent, Normal) * Normal);
  Bitangent = normalize(cross(Normal, Tangent)) * vertexTangent.w;

  TexCoord = vertexTexCoord;
  gl_Position = mvp * world_pos;
}
