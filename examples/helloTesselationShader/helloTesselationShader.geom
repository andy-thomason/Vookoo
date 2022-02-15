#version 420
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

layout(location = 0) in vec3 inColour[];
layout(location = 0) out vec3 fragColour;

void main()
{	
  vec4 center = (gl_in[0].gl_Position +
                 gl_in[1].gl_Position +
                 gl_in[2].gl_Position)/3.;

  for(int i=0; i<3; i++)
  {
    gl_Position = gl_in[i].gl_Position + (center-gl_in[i].gl_Position)*0.1;
    fragColour = inColour[i]; 
    EmitVertex();
  }
  EndPrimitive();
}
