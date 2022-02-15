#version 460 core
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified to be compatible with vulkan

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

//Vulkan uses shader I/O blocks, and we should redeclare a gl_PerVertex 
//block to specify exactly what members of this block to use. 
//When we don’t, the default definition is used. But we must remember 
//that the default definition contains gl_ClipDistance[], which requires 
//us to enable a feature named shaderClipDistance (and in Vulkan we 
//can’t use features that are not enabled during device creation or 
//our application may not work correctly). 
//Here we are using only a gl_Position member so the feature is not required.
in gl_PerVertex {
    vec4 gl_Position;
    //float gl_PointSize;
    //float gl_ClipDistance[];
    //float gl_CullDistance[];
} gl_in[];

out gl_PerVertex
{
  vec4 gl_Position;
  //float gl_PointSize;
  //float gl_ClipDistance[];
};

layout(location = 0) in vec3 tePosition[3];
layout(location = 1) in vec4 tePatchDistance[3];
layout(location = 2) in int tePrimitiveID[3];
layout(location = 0) out vec3 gFacetNormal;
layout(location = 1) out vec3 gTriDistance;
layout(location = 2) out vec4 gPatchDistance;
layout(location = 3) out int gPrimitiveID;

layout (binding = 0) uniform Uniform0 {
  mat4 Projection;
  mat4 View;
  mat4 GeometryTransform;
  float TessLevelInner;
  float TessLevelOuter;
  float PatchID;
  float padding;
} u0;

// instead, better if precompute uniform NormalMatrix on CPU
mat3 NormalMatrix = mat3(u0.View*u0.GeometryTransform); // may need transpose(inverse(...)) see http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/

void main()
{
    vec3 A = tePosition[2] - tePosition[0];
    vec3 B = tePosition[1] - tePosition[0];
    gFacetNormal = NormalMatrix * normalize(cross(A, B));

    gPrimitiveID = tePrimitiveID[0];    
    gPatchDistance = tePatchDistance[0];
    gTriDistance = vec3(1, 0, 0);
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();

    gPrimitiveID = tePrimitiveID[1];
    gPatchDistance = tePatchDistance[1];
    gTriDistance = vec3(0, 1, 0);
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();

    gPrimitiveID = tePrimitiveID[2];
    gPatchDistance = tePatchDistance[2];
    gTriDistance = vec3(0, 0, 1);
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();

    EndPrimitive();
}
