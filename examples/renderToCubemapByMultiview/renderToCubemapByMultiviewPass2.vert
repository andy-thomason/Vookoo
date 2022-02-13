#version 460

layout(location = 0) in vec3 a_position;

layout(location = 0) out vec3 v_direction;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
  mat4 projection;
  mat4 view;
  mat4 world;
} u;

out gl_PerVertex {
  vec4 gl_Position;
};

void main() {
  v_direction = normalize(a_position);
  gl_Position = u.projection * u.view * u.world * vec4(a_position, 1.0);
}
