#version 460

layout(location = 0) in vec3 v_direction;

layout(location = 0) out vec4 FragColor;

layout (binding = 1) uniform samplerCube u_cubemap;

void main() {
  FragColor = texture(u_cubemap, v_direction);
}
