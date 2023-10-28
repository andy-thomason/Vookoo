#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

// Per-instance attributes
layout (location = 4) in mat4 instanceModel;

layout(set = 0, binding = 0) uniform uboScene{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 v2fCol;
layout(location = 1) out vec2 v2fUV;
layout(location = 2) out vec3 v2fWorldPos;
layout(location = 3) out vec3 v2fWorldNormal;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * instanceModel * vec4(inPosition, 1.0);
    v2fWorldPos = vec3(ubo.model * instanceModel * vec4(inPosition, 1.0));
    v2fWorldNormal = vec3(transpose(inverse(ubo.model * instanceModel)) * vec4(inNormal, 1.0));
    v2fCol = inColor;
    v2fUV = inUV;
}
