#version 440 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

layout(location = 3) in vec3 instance_position;
layout(location = 4) in vec3 instance_scale;
layout(location = 5) in vec4 instance_rotation;

uniform mat4 u_mat_view;
uniform mat4 u_mat_projection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

mat3 quat_to_mat3(vec4 q) {
  float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
  float xx = q.x * x2, xy = q.x * y2, xz = q.x * z2;
  float yy = q.y * y2, yz = q.y * z2, zz = q.z * z2;
  float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;

  return mat3(
    vec3(1.0 - (yy + zz), xy + wz, xz - wy),
    vec3(xy - wz, 1.0 - (xx + zz), yz + wx),
    vec3(xz + wy, yz - wx, 1.0 - (xx + yy))
  );
}

void main() {
  mat3 R = quat_to_mat3(instance_rotation);
  mat4 model = mat4(
      vec4(R[0] * instance_scale.x, 0.0),
      vec4(R[1] * instance_scale.y, 0.0),
      vec4(R[2] * instance_scale.z, 0.0),
      vec4(instance_position, 1.0)
    );

  vec4 world_pos = model * vec4(position, 1.0);
  FragPos = world_pos.xyz;
  Normal = mat3(transpose(inverse(model))) * normal;
  TexCoord = texcoord;
  gl_Position = u_mat_projection * u_mat_view * world_pos;
}
