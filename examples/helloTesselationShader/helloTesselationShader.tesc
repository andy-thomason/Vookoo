#version 420
// https://web.engr.oregonstate.edu/~mjb/cs519/Handouts/tessellation.1pp.pdf
//    .tesc   for a tessellation control shader

layout( vertices = 3 ) out;

layout(location = 0) in  vec3 inColour[];
layout(location = 0) out vec3 outColour[3];

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

void main( )
{
  if (gl_InvocationID == 0)
  {
    float inner = 3.0;
    gl_TessLevelInner[0] = inner;
    float outer = 8.0;
    gl_TessLevelOuter[0] = outer;
    gl_TessLevelOuter[1] = outer;
    gl_TessLevelOuter[2] = outer;
  }

  gl_out[ gl_InvocationID ].gl_Position = gl_in[ gl_InvocationID ].gl_Position;
  outColour[ gl_InvocationID ] = inColour[ gl_InvocationID ];
}
