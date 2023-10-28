#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 v2fCol;
layout(location = 1) out vec2 v2fUV;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    v2fCol = vec3(inUV.x, inUV.y, 0.0);
    v2fUV = inUV;
}
