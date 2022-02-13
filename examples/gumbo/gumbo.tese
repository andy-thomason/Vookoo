#version 460 core
// original: https://prideout.net/blog/old/blog/index.html@p=49.html
// modified to be compatible with vulkan

layout( quads, ccw ) in;

layout(location = 0)  in vec3 tcPosition[];
layout(location = 0) out vec3 tePosition;
layout(location = 1) out vec4 tePatchDistance;
layout(location = 2) out int tePrimitiveID;

layout (binding = 0) uniform Uniform0 {
  mat4 Projection;
  mat4 View;
  mat4 GeometryTransform;
  float TessLevelInner;
  float TessLevelOuter;
  float PatchID;
  float padding;
} u0;

mat4 B = mat4(
  vec4(-1, 3,-3, 1),
  vec4( 3,-6, 3, 0),
  vec4(-3, 3, 0, 0),
  vec4( 1, 0, 0, 0)
);
mat4 BT = transpose(B);

in gl_PerVertex
{
  vec4 gl_Position;
  //float gl_PointSize;
  //float gl_ClipDistance[];
} gl_in[gl_MaxPatchVertices];

out gl_PerVertex {
  vec4 gl_Position;
  //float gl_PointSize;
  //float gl_ClipDistance[];
};

//https://ogldev.org/www/tutorial30/tutorial30.html
//fixed inconsistent winding order of https://prideout.net/blog/old/blog/index.html@p=49.html gumbo.h patches [97,98,...108] by transposing each Px, Py and Pz

void main()
{
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    mat4 Px = mat4(
        tcPosition[0].x, tcPosition[1].x, tcPosition[2].x, tcPosition[3].x, 
        tcPosition[4].x, tcPosition[5].x, tcPosition[6].x, tcPosition[7].x, 
        tcPosition[8].x, tcPosition[9].x, tcPosition[10].x, tcPosition[11].x, 
        tcPosition[12].x, tcPosition[13].x, tcPosition[14].x, tcPosition[15].x );

    mat4 Py = mat4(
        tcPosition[0].y, tcPosition[1].y, tcPosition[2].y, tcPosition[3].y, 
        tcPosition[4].y, tcPosition[5].y, tcPosition[6].y, tcPosition[7].y, 
        tcPosition[8].y, tcPosition[9].y, tcPosition[10].y, tcPosition[11].y, 
        tcPosition[12].y, tcPosition[13].y, tcPosition[14].y, tcPosition[15].y );

    mat4 Pz = mat4(
        tcPosition[0].z, tcPosition[1].z, tcPosition[2].z, tcPosition[3].z, 
        tcPosition[4].z, tcPosition[5].z, tcPosition[6].z, tcPosition[7].z, 
        tcPosition[8].z, tcPosition[9].z, tcPosition[10].z, tcPosition[11].z, 
        tcPosition[12].z, tcPosition[13].z, tcPosition[14].z, tcPosition[15].z );

    mat4 cx = B * Px * BT;
    mat4 cy = B * Py * BT;
    mat4 cz = B * Pz * BT;
    
    vec4 U = vec4(u*u*u, u*u, u, 1);
    vec4 V = vec4(v*v*v, v*v, v, 1);

    float x = dot(cx * V, U);
    float y = dot(cy * V, U);
    float z = dot(cz * V, U);
    
    tePrimitiveID = gl_PrimitiveID;
    tePosition =  vec3(x, y, z);
    tePatchDistance = vec4(u, v, 1-u, 1-v);
    gl_Position = u0.Projection * u0.View * u0.GeometryTransform * vec4(tePosition, 1.);
}
