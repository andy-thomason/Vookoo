#version 450

layout(location = 0) in vec3 v2fCol;
layout(location = 1) in vec2 v2fUV;
layout(location = 2) in vec2 v2fScreenUV;
layout(location = 3) in vec3 v2fWorldPos;
layout(location = 4) in vec3 v2fWorldNormal;

layout(set = 0, binding = 0) uniform uboFX {
  mat4 model;
  mat4 view;
  mat4 proj;
  vec2 res;
} ubo;

//layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputColorAttachment;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputDepthAttachment;

layout(location = 0) out vec4 outColor;

vec3 rand3(vec3 co){
  return vec3(fract(sin(dot(co.xyz ,vec3(12.9898,78.233,33.19534))) * 43758.5453));
}

// Minimal Hexagonal Shader/Grid
// Created by leftofzen in 2020-02-12 
// https://www.shadertoy.com/view/wtdSzX
const vec2 s = vec2(1, 1.7320508);

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(141.13, 289.97)))*43758.5453);
}

// The 2D hexagonal isosuface function: If you were to render a horizontal line and one that
// slopes at 60 degrees, mirror, then combine them, you'd arrive at the following. As an aside,
// the function is a bound -- as opposed to a Euclidean distance representation, but either
// way, the result is hexagonal boundary lines.
float hex(in vec2 p)
{    
    p = abs(p);
    
    #ifdef FLAT_TOP_HEXAGON
    return max(dot(p, s*.5), p.y); // Hexagon.
    #else
    return max(dot(p, s*.5), p.x); // Hexagon.
    #endif    
}

// This function returns the hexagonal grid coordinate for the grid cell, and the corresponding 
// hexagon cell ID -- in the form of the central hexagonal point. That's basically all you need to 
// produce a hexagonal grid.
//
// When working with 2D, I guess it's not that important to streamline this particular function.
// However, if you need to raymarch a hexagonal grid, the number of operations tend to matter.
// This one has minimal setup, one "floor" call, a couple of "dot" calls, a ternary operator, etc.
// To use it to raymarch, you'd have to double up on everything -- in order to deal with 
// overlapping fields from neighboring cells, so the fewer operations the better.
vec4 getHex(vec2 p)
{    
    // The hexagon centers: Two sets of repeat hexagons are required to fill in the space, and
    // the two sets are stored in a "vec4" in order to group some calculations together. The hexagon
    // center we'll eventually use will depend upon which is closest to the current point. Since 
    // the central hexagon point is unique, it doubles as the unique hexagon ID.
    
    #ifdef FLAT_TOP_HEXAGON
    vec4 hC = floor(vec4(p, p - vec2(1, .5))/s.xyxy) + .5;
    #else
    vec4 hC = floor(vec4(p, p - vec2(.5, 1))/s.xyxy) + .5;
    #endif
    
    // Centering the coordinates with the hexagon centers above.
    vec4 h = vec4(p - hC.xy*s, p - (hC.zw + .5)*s);
    
    
    // Nearest hexagon center (with respect to p) to the current point. In other words, when
    // "h.xy" is zero, we're at the center. We're also returning the corresponding hexagon ID -
    // in the form of the hexagonal central point.
    //
    // On a side note, I sometimes compare hex distances, but I noticed that Iomateron compared
    // the squared Euclidian version, which seems neater, so I've adopted that.
    return dot(h.xy, h.xy) < dot(h.zw, h.zw) 
        ? vec4(h.xy, hC.xy) 
        : vec4(h.zw, hC.zw + .5);
}

void main()
{
  // calculate attachment's world-space position
  vec2 ndc_xy = v2fScreenUV * 2.0 - 1.0; // remap to [-1,1]
  float inDepth = subpassLoad(inputDepthAttachment).x * 2.0 - 1.0; // remap to [-1, 1]
  vec4 inAttachment_ndc = vec4(ndc_xy, inDepth, 1.0); 
  
  vec4 ViewDepth = inverse(ubo.proj) * inAttachment_ndc;
  ViewDepth /= ViewDepth.w;

  vec4 ViewPos = ubo.view * vec4(v2fWorldPos,1.);
  ViewPos /= ViewPos.w;
  
  vec3 worldNormal = normalize(v2fWorldNormal);
  vec4 viewNormal = transpose(inverse(ubo.view)) * vec4(worldNormal,1.);
  viewNormal /= viewNormal.w;
  viewNormal.xyz = normalize(viewNormal.xyz);

  float alpha = 1.-smoothstep(0.,0.9,abs(dot(viewNormal.xyz,vec3(0,0,1))));
  float beta = length(ViewDepth.z-ViewPos.z) < 0.1 ? 0.3 : 0.0;
  float a0 = smoothstep(0.2,0.6,alpha);
  float b0 = smoothstep(0.2,0.5,alpha);

  outColor = vec4(subpassLoad(inputColorAttachment).xyz,0.75);
  outColor.xyz += vec3(beta,0.,0.);
  outColor.xyz += vec3(0.,0.,alpha);
  outColor.xyz += vec3(a0,0.,a0);
  outColor.xyz += vec3(0.,b0,0.);
  //outColor.xyz += vec3(beta+a0,b0,alpha+a0);
  //outColor.xyz += sin(subpassLoad(inputColorAttachment).xyz*6/.01);
  //outColor.xyz += rand3(sin(subpassLoad(inputColorAttachment).xyz*6/.01));
  //outColor.xyz += sin(ViewDepth.xyz*6/1.1);
  outColor.xyz += rand3(sin(ViewDepth.xyz*6/.1))/10.;

  vec4 P = mat4(mat3(ubo.view * ubo.model)) * vec4(viewNormal.xyz,1.);
  float theta = acos(P.z); // [0,pi]
  float phi = atan(P.y,P.x); // [-pi,pi]
  if (phi < 0.) {
    phi += 6.2914;
  }
  vec2 st = vec2(phi/6.2914,theta/3.1457);

  vec4 h = getHex(st*50.);
  //vec4 h = getHex(viewNormal.xy*5.);
  // The beauty of working with hexagonal centers is that the relative edge distance will simply 
  // be the value of the 2D isofield for a hexagon.
  float eDist = hex(h.xy); // Edge distance.
  // Initiate the background to a white color, putting in some dark borders.
  vec3 col = mix(vec3(1.), vec3(0), smoothstep(0., .03, eDist - .5 + .04));
  outColor.xyz *= col;

/*
  theta = acos(-P.z); // [0,pi]
  phi = atan(P.y,P.x); // [-pi,pi]
  if (phi < 0.) {
    phi += 6.2914;
  }
  st = vec2(phi/6.2914,theta/3.1457);
  //h = getHex(vec2(worldNormal.y,worldNormal.x)*5.);
  h = getHex(st*5.);
  eDist = hex(h.xy); // Edge distance.
  col = mix(vec3(1.), vec3(0), smoothstep(0., .03, eDist - .5 + .04));
  outColor.xyz *= col;
*/
}

void main1()
{
  // calculate attachment's world-space position
  vec2 ndc_xy = v2fScreenUV * 2.0 - 1.0; // remap to [-1,1]
  float inDepth = subpassLoad(inputDepthAttachment).x * 2.0 - 1.0; // remap to [-1, 1]
  vec4 inAttachment_ndc = vec4(ndc_xy, inDepth, 1.0); 
  
  mat4 inv_viewProj = inverse(ubo.proj * ubo.view);
  vec4 inWorld_w = inv_viewProj * inAttachment_ndc;
  vec3 inWorld   = inWorld_w.xyz / inWorld_w.w;
  
  // compare attachment's and fragment's worldspace positions
//  float glow      = smoothstep(0.0,1.5,1.0-(inWorld.z-(v2fWorldPos.z-0.65)));
//  float highlight = smoothstep(0.0,1.5,1.0-(inWorld.z-(v2fWorldPos.z-0.75)));
//  float intersect = smoothstep(0.0,1.0,1.0-(inWorld.z-(v2fWorldPos.z-0.80)));

vec4 z = vec4(0.0, 0.0, 1.0, 1.0);
z = ubo.view * z;

  float glow      = smoothstep(0.0,1.5,1.0-(dot(inWorld-v2fWorldPos,z.xyz)-0.65));
  float highlight = smoothstep(0.0,1.5,1.0-(dot(inWorld-v2fWorldPos,z.xyz)-0.75));
  float intersect = smoothstep(0.0,1.0,1.0-(dot(inWorld-v2fWorldPos,z.xyz)-0.80));
  
  vec3 worldNormal = normalize(v2fWorldNormal);
//  vec4 z = vec4(0.0, 0.0, -1.0, 1.0);
//  z = ubo.view * z;
  float dotprod = dot(worldNormal, -z.xyz);
  float angle = abs(acos(dotprod));
  float outline = smoothstep(3.14*0.25, 3.14*0.5, angle);

  vec3 col = mix(vec3(0.0),vec3(1.0,1.0,10.0), outline+glow);
  col     += mix(vec3(0.0),vec3(1.0,1.0,10.0), highlight + highlight);
  col     += mix(vec3(0.0),vec3(1.0,1.0,10.0), intersect * 4.0 );
  outColor = vec4(col, 0.75);
//outColor = vec4(vec3(intersect), 0.75);
}

void main0()
{
  // calculate attachment's world-space position
  vec2 ndc_xy = v2fScreenUV * 2.0 - 1.0; // remap to [-1,1]
  float inDepth = subpassLoad(inputDepthAttachment).x * 2.0 - 1.0; // remap to [-1, 1]
  vec4 inAttachment_ndc = vec4(ndc_xy, inDepth, 1.0); 
  
  mat4 inv_viewProj = inverse(ubo.proj * ubo.view);
  vec4 inWorld_w = inv_viewProj * inAttachment_ndc;
  vec3 inWorld   = inWorld_w.xyz / inWorld_w.w;
  
  // compare attachment's and fragment's worldspace positions
  float glow      = smoothstep(0.0,1.5,1.0-(inWorld.z-(v2fWorldPos.z-0.65)));
  float highlight = smoothstep(0.0,1.5,1.0-(inWorld.z-(v2fWorldPos.z-0.75)));
  float intersect = smoothstep(0.0,1.0,1.0-(inWorld.z-(v2fWorldPos.z-0.80)));
  
  vec3 worldNormal = normalize(v2fWorldNormal);
  float dotprod = dot(worldNormal, vec3(0.0, 0.0, -1.0));
  float angle = abs(acos(dotprod));
  float outline = smoothstep(3.14*0.25, 3.14*0.5, angle);

  vec3 col = mix(vec3(0.0),vec3(1.0,1.0,10.0), outline+glow);
  col     += mix(vec3(0.0),vec3(1.0,1.0,10.0), highlight + highlight);
  col     += mix(vec3(0.0),vec3(1.0,1.0,10.0), intersect * 4.0 );
  outColor = vec4(col, 0.75);
}
