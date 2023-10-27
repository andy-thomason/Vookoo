#version 450

layout(location = 0) in vec3 v2fCol;
layout(location = 1) in vec2 v2fUV;
layout(location = 2) in vec2 v2fScreenUV;
layout(location = 3) in vec3 v2fWorldPos;
layout(location = 4) in vec3 v2fWorldNormal;

layout(set = 0, binding = 0) uniform uboFX{
  mat4 model;
  mat4 view;
  mat4 proj;
  vec2 res;
} ubo;

//layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputColorAttachment;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputDepthAttachment;

layout(location = 0) out vec4 outColor;


void main() {

  float inDepth = subpassLoad(inputDepthAttachment).x * 2. - 1.; // depth is [-1, 1]
  //vec3 inCol = subpassLoad(inputColorAttachment).xyz; 
  
  // calculate scene view-space position:
  
  // get ndc position from screen XY positions, and depth buffer Z position.  (post perspective divide)
  vec2 ndc_xy = v2fScreenUV * 2.0 - 1.0; // remap to [-1,1]  (Vulkan coordinate system is [-1, 1])
  vec4 sceneNDC = vec4(ndc_xy, inDepth, 1.0); 
  

  // transform NDC to viewspace position
  mat4 invProj = inverse(ubo.proj);
  vec4 sceneViewPos_w = invProj * sceneNDC;
  vec3 sceneViewPos  = sceneViewPos_w.xyz / sceneViewPos_w.w;

  vec3 camWorldPos =  vec3(0.0f, 1.0f, 4.0f);
  vec3 viewCenter = vec3(0.0f, 0.0f, 1.0f);
  vec3 camWorldDir = normalize(viewCenter - camWorldPos);
  
  // get scene's worldspace position
  vec3 fragWorldDir = normalize(  v2fWorldPos - camWorldPos );
  
  float dot = dot(fragWorldDir, camWorldDir);
  vec3 a = fragWorldDir / dot;
  vec3 b = -sceneViewPos.z * a;
  
  vec3 sceneWorldPos = camWorldPos + b;

  // transform scene's worldspace to decal's object space
  vec3 sceneModelPos = vec3(inverse(ubo.model) * vec4(sceneWorldPos, 1.0));


  // bound-box clipping in decal object space
  float cubeEdgeLength = 4.0;
  vec3 bounds = vec3(cubeEdgeLength / 2.0);
  vec3 xyz = abs(sceneModelPos);
  vec3 testBounds = (xyz - bounds);
  if(testBounds.x >= 0.0 || testBounds.y >= 0.0 || testBounds.z >= 0.0 ) {discard;}
  
  float dist = length(abs(sceneModelPos.xyz));
  float y = dist/bounds.x;
  
  float range = 0.74;
  float blurOuter = 0.25;
  float blurInner = 0.025;
  float outer = smoothstep(range, range + blurOuter, y) * step(range, y);
  float outer1 = smoothstep(range, range + 0.0125, y) * step(range, y);
  float inner = (1.0-smoothstep(range - blurInner, range, y)) * (1.0-step(range, y));
  
  float mask0 =  (1.0-smoothstep(range, range + blurOuter, y)) * step(range, y);
  float mask1 =  (smoothstep(range - blurInner, range, y)) * (1.0-step(range, y));
  float mask3 =  (1.0-smoothstep(range, range + 0.0125, y)) * step(range, y);
  float mask = mask0*0.125 + mask1*0.75 + mask3*0.75 + (1.0-step(range,y))*.125;
  
  vec3 col = mix(vec3(1.0), vec3(0.0,0.0,1.0), outer+outer1+inner);
  col.xyz = mix(vec3(0.0,0.0,0.5)*.75, col.xyz, step(range-0.125, y));
  outColor = vec4(col, mask);
}
