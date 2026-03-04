#version 440 core
layout(points) in;
layout(triangle_strip, max_vertices = 128) out;

uniform uint u_stacks;
uniform uint u_slices;
uniform float u_radius;

out vec3 fragNormal;
out vec3 fragPos;

void main() {
  vec3 center = gl_in[0].gl_Position.xyz;

  for (int i = 0; i < u_stacks; i++) {
    float theta1 = float(i) / float(u_stacks) * 3.1415926;
    float theta2 = float(i + 1) / float(u_stacks) * 3.1415926;

    for (int j = 0; j <= u_slices; j++) {
      float phi = float(j) / float(u_slices) * 2.0 * 3.1415926;

      vec3 p1 = center + u_radius * vec3(
              sin(theta1) * cos(phi),
              cos(theta1),
              sin(theta1) * sin(phi)
            );

      vec3 p2 = center + u_radius * vec3(
              sin(theta2) * cos(phi),
              cos(theta2),
              sin(theta2) * sin(phi)
            );

      fragPos = p1;
      fragNormal = normalize(p1 - center);
      gl_Position = vec4(p1, 1.0);
      EmitVertex();

      fragPos = p2;
      fragNormal = normalize(p2 - center);
      gl_Position = vec4(p2, 1.0);
      EmitVertex();
    }
    EndPrimitive();
  }
}
