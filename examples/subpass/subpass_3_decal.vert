#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

// Per-instance attributes
layout (location = 4) in mat4 instanceModel;

layout(set = 0, binding = 0) uniform uboFX{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec2 res;
} ubo;

layout(location = 0) out vec3 v2fCol;
layout(location = 1) out vec2 v2fUV;
layout(location = 2) out vec2 v2fScreenUV;
layout(location = 3) out vec3 v2fWorldPos;
layout(location = 4) out vec3 v2fWorldNormal;

void main() {
    vec4 clipPos = ubo.proj * ubo.view * ubo.model * instanceModel * vec4(inPosition, 1.0); // store clip-space position (before perspective divide)
    gl_Position = clipPos;
    v2fCol = inColor;
    v2fUV = inUV;
 
    // Vulkan coordinate system is [-1, 1]
    vec3 ndc = clipPos.xyz / clipPos.w; 
    
    // For screenspace UV, remap to [0,1]
    v2fScreenUV = ndc.xy * 0.5 + vec2(0.5);
    
    v2fWorldPos = vec3(ubo.model * instanceModel * vec4(inPosition, 1.0));
//    v2fWorldNormal =  vec3( ubo.model * vec4(inNormal, 1.0));
    v2fWorldNormal = vec3(transpose(inverse(ubo.model * instanceModel)) * vec4(inNormal, 1.0));
}
