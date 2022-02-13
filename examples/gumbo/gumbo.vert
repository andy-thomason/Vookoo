#version 460 core
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified to be compatible with vulkan

layout(location = 0) in vec4 Position;

layout(location = 0) out vec3 vPosition;

layout (binding = 0) uniform Uniform0 {
  mat4 Projection;
  mat4 View;
  mat4 GeometryTransform;
  float TessLevelInner;
  float TessLevelOuter;
  float PatchID;
  float padding;
} u0;

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

void main()
{
    vPosition = Position.xyz;
}
