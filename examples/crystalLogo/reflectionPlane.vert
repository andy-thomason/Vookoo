#version 460

layout(location = 0) in vec3 a_position;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
 mat4 u_textureMatrix;
 mat4 u_world;
} u;

#define u_textureMatrix u.u_textureMatrix
#define u_world u.u_world

layout(location = 0) out vec4 vUv;

void main() {
  vUv = u_textureMatrix * vec4(a_position, 1.0);

  gl_Position = u_world * vec4(a_position, 1.0);
}
