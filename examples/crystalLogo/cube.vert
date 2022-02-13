#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_center;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec3 a_color;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
 mat4 u_projection;
 mat4 u_view;
 mat4 u_world;
} u;

#define u_projection u.u_projection
#define u_view u.u_view
#define u_world u.u_world

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_center;
layout(location = 2) out vec3 v_point;
layout(location = 3) out vec2 v_uv;
layout(location = 4) out vec3 v_color;
layout(location = 5) out float v_depth;

void main() {
  vec4 center = u_projection * u_view * u_world * vec4(a_center, 1.0);
  vec4 position = u_projection * u_view * u_world * vec4(a_position, 1.0);

  v_normal = normalize(a_position);
  v_center = center.xyz;
  v_point = position.xyz;
  v_uv = a_uv;
  v_color = a_color;
  v_depth = (mat3(u_view) * mat3(u_world) * a_position).z;

  gl_Position = position;
}
