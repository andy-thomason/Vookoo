#version 460 core
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified to be compatible with vulkan

layout( vertices = 16 ) out;

layout(location = 0) in  vec3 vPosition[];
layout(location = 0) out vec3 tcPosition[];

layout (binding = 0) uniform Uniform0 {
  mat4 Projection;
  mat4 View;
  mat4 GeometryTransform;
  float TessLevelInner;
  float TessLevelOuter;
  float PatchID;
  float padding;
} u0;

in gl_PerVertex
{
  vec4 gl_Position;
  //float gl_PointSize;
  //float gl_ClipDistance[];
} gl_in[gl_MaxPatchVertices];

out gl_PerVertex
{
  vec4 gl_Position;
  //float gl_PointSize;
  //float gl_ClipDistance[];
} gl_out[];

#define ID gl_InvocationID

void main()
{
    tcPosition[ID] = vPosition[ID];

    //barrier();
    if (ID == 0) {
        gl_TessLevelInner[0] = u0.TessLevelInner;
        gl_TessLevelInner[1] = u0.TessLevelInner;
        gl_TessLevelOuter[0] = u0.TessLevelOuter;
        gl_TessLevelOuter[1] = u0.TessLevelOuter;
        gl_TessLevelOuter[2] = u0.TessLevelOuter;
        gl_TessLevelOuter[3] = u0.TessLevelOuter;
    }
}
