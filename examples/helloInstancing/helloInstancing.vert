#version 460

// Vertex attributes
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColour;

// Per-instance attributes
layout (location = 2) in vec3 instancePos;
layout (location = 3) in vec3 instanceRot;
layout (location = 4) in float instanceScale;

// Copy colour to the fragment shader.
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

mat3 instanceRotation() {
	mat3 mx, my, mz;
  float c, s;
	
	// rotate around x
	s = sin(instanceRot.x);
	c = cos(instanceRot.x);
	mx[0] = vec3(1.0, 0.0, 0.0);
	mx[1] = vec3(0.0,   c,   s);
	mx[2] = vec3(0.0,  -s,   c);
	
	// rotate around y
	s = sin(instanceRot.y);
	c = cos(instanceRot.y);
	my[0] = vec3(  c, 0.0,   s);
	my[1] = vec3(0.0, 1.0, 0.0);
	my[2] = vec3( -s, 0.0,   c);
	
	// rot around z
	s = sin(instanceRot.z);
	c = cos(instanceRot.z);	
	mz[0] = vec3(  c,   s, 0.0);
	mz[1] = vec3( -s,   c, 0.0);
	mz[2] = vec3(0.0, 0.0, 1.0);
	
  return mz * my * mx;
}

void main() {
  gl_Position = vec4(instanceRotation() * instanceScale * vec3(inPosition.xy,0.0) + instancePos, 1.0);
  fragColour = inColour;                    
}
