#version 460

layout(location = 0) in vec4 v_color;

layout(location = 0) out vec4 FragColor;

void main() {
  FragColor = v_color;
}
