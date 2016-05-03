#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec4 pos_in;
layout (location = 1) in vec3 normal_in;
layout (location = 2) in vec2 uv_in;

layout (binding = 0) uniform UBO 
{
	mat4 viewToProjection;
	mat4 modelToWorld;
	mat4 worldToView;
	mat4 normalToWorld;
	vec4 worldLightPosition;
} ubo;

layout (location = 0) out vec3 normal_out;
layout (location = 1) out vec3 viewPos_out;
layout (location = 2) out vec3 viewLightPos_out;

void main() {
  mat4 modelToView = ubo.worldToView * ubo.modelToWorld;
	normal_out = normalize(mat3(ubo.normalToWorld) * normal_in);
	vec4 viewPos = modelToView * pos_in;
  viewPos_out = viewPos.xyz;
  viewLightPos_out = (modelToView * ubo.worldLightPosition).xyz;
	gl_Position = ubo.viewToProjection * viewPos;

	/*mat4 modelToView = ubo.worldToView * ubo.modelToWorld;
	vec4 viewPos = modelToView * pos_in;	
	eyeDirection_out = -viewPos.xyz;
	vec4 viewLightPos = ubo.worldToView * ubo.worldLightPosition;
	lightDirection_out = normalize(viewLightPos.xyz - viewPos.xyz);*/
}
