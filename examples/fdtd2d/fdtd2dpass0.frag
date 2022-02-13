#version 460
precision highp float;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
  vec4 iResolution; // viewport resolution (in pixels)
  int iFrame[4]; // shader playback frame
  vec4 iChannelResolution[4]; // channel resolution (in pixels), only uses [0] and [1]
} u;

#define iResolution u.iResolution
#define iFrame u.iFrame[0]
#define iChannelResolution u.iChannelResolution

layout (binding = 1) uniform sampler2D iChannel0; // Buffer A; 4ch, float32, linear, repeat
layout (binding = 2) uniform sampler2D iChannel1; // Buffer B; 4ch, float32, linear, repeat

layout(location = 0) out vec4 outColour;

// created by pocdn - 2021
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// 2D Finite-Difference Time-Domain 
// ---------------
// Simpliest case with periodic boundary conditions 

const float cc=2.99792458e8;
const float muz=4.0*3.14*1.0e-7;
const float epsz=1.0/(cc*cc*muz);
const float etaz=sqrt(muz/epsz);

const float freq=5.0e+9;
const float lambda=cc/freq;
const float omega=2.0*3.14*freq;


const float dx=3.0e-3;
const float dt=dx/(2.0*cc);

const float eaf = dt*0./(2.*epsz);
const float ca = (1.-eaf)/(1.+eaf);
const float cb=dt/epsz/dx/(1.0+eaf);
const float haf  =dt*1./(2.0*muz);
const float da=(1.0-haf)/(1.0+haf);
const float db=dt/muz/dx/(1.0+haf);

vec4 getE(vec2 p){
    return texture(iChannel0,p/iResolution.xy);
}

vec4 getH(vec2 p){
    return texture(iChannel1,p/iResolution.xy);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ){
    
    vec4 e =   getE(fragCoord);
    vec4 h =   getH(fragCoord);
    vec4 hmY = getH(fragCoord+vec2( 0,-1));
    vec4 hmX = getH(fragCoord+vec2(-1, 0));
    
    //ex(i,j)=caex(i,j)*ex(i,j)+cbex(i,j)*(hz(i,j)-hz(i,j-1));
    fragColor.x = ca*e.x + cb*(h.z-hmY.z);
    
    //ey(i,j)=caey(i,j)*ey(i,j)+cbey(i,j)*(hz(i-1,j)-hz(i,j));
    fragColor.y = ca*e.y + cb*(hmX.z-h.z);
}

void main() {
  mainImage( outColour, (iResolution.xy/iChannelResolution[0].xy)*gl_FragCoord.xy );
}

