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

vec4 color_map(float s, float div) {
    // credit: https://www.shadertoy.com/view/WlfXRN
    //         https://observablehq.com/@flimsyhat/webgl-color-maps
    // slightly modified viridis
    float t = s/div*0.5+0.5;

    const vec3 c0 = vec3(0.2777273272234177, 0.005407344544966578, 0.3340998053353061);
    const vec3 c1 = vec3(0.1050930431085774, 1.404613529898575, 1.384590162594685);
    const vec3 c2 = vec3(-0.3308618287255563, 0.214847559468213, 0.09509516302823659);
    const vec3 c3 = vec3(-4.634230498983486, -5.799100973351585, -19.33244095627987);
    const vec3 c4 = vec3(6.228269936347081, 14.17993336680509, 56.69055260068105);
    const vec3 c5 = vec3(4.776384997670288, -13.74514537774601, -65.35303263337234);
    const vec3 c6 = vec3(-5.435455855934631, 4.645852612178535, 26.3124352495832);

    return vec4(c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6))))),1.);

}

// created by pocdn - 2021
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// 2D Finite-Difference Time-Domain 
// ---------------
// Simpliest case with periodic boundary conditions 

vec4 getE(vec2 p){
    return texture(iChannel0,p/iResolution.xy);
}

vec4 getH(vec2 p){
    return texture(iChannel1,p/iResolution.xy);
}

vec4 colormap(float s, float div) {
    return vec4(vec3(s/div*0.5+0.5),1);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ){
    vec4 h = getH(fragCoord);
	  fragColor = color_map(h.z, 5.);
}

void main() {
  mainImage( outColour, gl_FragCoord.xy );
}

