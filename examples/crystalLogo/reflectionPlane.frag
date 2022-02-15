#version 460

layout (binding = 1) uniform samplerCube u_texture; // https://satellitnorden.wordpress.com/2018/01/23/vulkan-adventures-cube-map-tutorial/

layout(location = 0) in vec4 vUv;

layout(location = 0) out vec4 FragColor;

void main() {
  FragColor = texture(u_texture, vUv.xyz); // https://satellitnorden.wordpress.com/2018/01/23/vulkan-adventures-cube-map-tutorial/
}
