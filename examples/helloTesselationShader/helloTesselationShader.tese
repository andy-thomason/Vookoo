#version 420
// https://web.engr.oregonstate.edu/~mjb/cs519/Handouts/tessellation.1pp.pdf
//    .tese   for a tessellation evaluation shader

layout( triangles, equal_spacing ) in;

layout(location = 0)  in vec3 patch_color[];
layout(location = 0) out vec3 tes_color;

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

void main( )
{
  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position +
                gl_TessCoord.y * gl_in[1].gl_Position +
                gl_TessCoord.z * gl_in[2].gl_Position;
  tes_color = gl_TessCoord.x * patch_color[0] + 
              gl_TessCoord.y * patch_color[1] + 
              gl_TessCoord.z * patch_color[2];
}
