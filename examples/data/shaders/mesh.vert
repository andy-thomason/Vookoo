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
	vec3 lightPosition;
} ubo;

layout (location = 0) out vec3 normal_out;
layout (location = 1) out vec3 eyeDirection_out;
layout (location = 2) out vec3 lightDirection_out;

void main() {
	normal_out = normalize(mat3(ubo.normalToWorld) * normal_in);
	mat4 modelToView = ubo.worldToView * ubo.modelToWorld;
	vec4 viewPos = modelToView * pos_in;	
	eyeDirection_out = -viewPos;
	vec4 lightPos = vec4(ubo.lightPosition, 1.0) * worldToView;
	lightDirection_out = normalize(lightPos.xyz - eyePos_out);
	gl_Position = ubo.viewToProjection * viewPos;
}
