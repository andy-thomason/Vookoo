#version 460

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
  vec4 iResolution; // viewport resolution (in pixels)
  int iFrame[4]; // shader playback frame
  vec4 iChannelResolution[4]; // channel resolution (in pixels), only uses [0] and [1]
} u;

#define iResolution u.iResolution
#define iFrame u.iFrame[0]
#define iChannelResolution u.iChannelResolution

layout (binding = 1) uniform sampler2D iChannel0; // Buffer A; 4ch, float32, linear, clamp
layout (binding = 2) uniform sampler2D iChannel1; // initial texture i.e. random values, mipmap, repeat, vflip

layout(location = 0) out vec4 outColour;

// created by florian berger (flockaroo) - 2016
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// single pass CFD
// ---------------
// this is some "computational flockarooid dynamics" ;)
// the self-advection is done purely rotational on all scales. 
// therefore i dont need any divergence-free velocity field. 
// with stochastic sampling i get the proper "mean values" of rotations 
// over time for higher order scales.
//
// try changing "RotNum" for different accuracies of rotation calculation
// for even RotNum uncomment the line #define SUPPORT_EVEN_ROTNUM

float getVal(vec2 uv)
{
    return length(texture(iChannel0,uv).xyz);
}
    
vec2 getGrad(vec2 uv,float delta)
{
    vec2 d=vec2(delta,0);
    return vec2(
        getVal(uv+d.xy)-getVal(uv-d.xy),
        getVal(uv+d.yx)-getVal(uv-d.yx)
    )/delta;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    vec3 n = vec3(getGrad(uv,1.0/iResolution.y),150.0);
    //n *= n;
    n=normalize(n);
    fragColor=vec4(n,1);
    vec3 light = normalize(vec3(1,1,2));
    float diff=clamp(dot(n,light),0.5,1.0);
    float spec=clamp(dot(reflect(light,n),vec3(0,0,-1)),0.0,1.0);
    spec=pow(spec,36.0)*2.5;
    //spec=0.0;
	fragColor = texture(iChannel0,uv)*vec4(diff)+vec4(spec);
}

void main() {
  mainImage( outColour, gl_FragCoord.xy );
}
