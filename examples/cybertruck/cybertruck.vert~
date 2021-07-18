#version 460

layout(location = 0) in vec2 inPosition;

//Vulkan uses shader I/O blocks, and we should redeclare a gl_PerVertex 
//block to specify exactly what members of this block to use. 
//When we don’t, the default definition is used. But we must remember 
//that the default definition contains gl_ClipDistance[], which requires 
//us to enable a feature named shaderClipDistance (and in Vulkan we 
//can’t use features that are not enabled during device creation or 
//our application may not work correctly). 
//Here we are using only a gl_Position member so the shaderClipDistance
//feature is not required.
out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    // Copy 2D position to 3D + depth
    gl_Position = vec4(inPosition, 0.5, 1.0); // units [clip coords]
}
