#version 460

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform PER_OBJECT {
  mat4 MVP;
} obj;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;

layout(location = 0) out vec3 fragColour;

//Vulkan uses shader I/O blocks, and we should redeclare a gl_PerVertex 
//block to specify exactly what members of this block to use. 
//When we don’t, the default definition is used. But we must remember 
//that the default definition contains gl_ClipDistance[], which requires 
//us to enable a feature named shaderClipDistance (and in Vulkan we 
//can’t use features that are not enabled during device creation or 
//our application may not work correctly). 
//Here we are using only a gl_Position member so the feature is not required.
out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
  gl_Position = obj.MVP * vec4(inPosition, 0.0, 1.0); // Copy 2D position to 3D + depth
  fragColour = inColour;                    // Copy colour to the fragment shader.
}
