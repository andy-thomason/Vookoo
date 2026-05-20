#version 460

// https://www.shadertoy.com/view/wdGXzK
// Wait.. what? CyberTruck!
// Tags: teslacybertruck
// Created by BigWIngs in 2019-11-28

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
    vec4 iResolution; // viewport resolution (in pixels)
    vec4 iTime; //  render time (in seconds).
    ivec4 iFrame; // shader playback frame
    vec4 iMouse; // mouse pixel coords. xy: current (if MLB down), zw: click
} u;

#define iResolution u.iResolution
#define iTime u.iTime[0]
#define iFrame u.iFrame[0]
#define iMouse u.iMouse

layout(location = 0) out vec4 outColour;

void mainImage( out vec4 fragColor, in vec2 fragCoord );

void main() {
    // Original shadertoy fragment shader written assuming OpenGL standard
    // where pixel origin is 0,0 at LOWER-left corner and
    // upper right corner is iResolution.x,iResolution.y
    // But here, in Vulkan origin 0,0 is UPPER-left corner so 
    // the transform maps from GL(x,y) to VULKAN(x,iResolution.y-y).
    // Note, viewport, cullMode, and FrontFace do not change this gl_FragCoord.
    // Vulkan requires gl_FragCoord’s origin to be at the top left corner -- non-changeable
    vec2 openGLFragCoord = vec2(gl_FragCoord.x, iResolution.y-gl_FragCoord.y);
    mainImage( outColour, openGLFragCoord );
}

//////////////////////////////////////////////////////////////////////////

float sabs(float x,float k) {
    float a = (.5/k)*x*x+k*.5;
    float b = abs(x);
    return b<k ? a : b;
}
vec2 sabs(vec2 x,float k) { return vec2(sabs(x.x, k), sabs(x.y,k)); }
vec3 sabs(vec3 x,float k) { return vec3(sabs(x.x, k), sabs(x.y,k), sabs(x.z,k)); }

mat2 Rot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

float smin( float a, float b, float k ) {
    float h = clamp( 0.5+0.5*(b-a)/k, 0., 1. );
    return mix( b, a, h ) - k*h*(1.0-h);
}

// From http://mercury.sexy/hg_sdf
vec2 pModPolar(inout vec2 p, float repetitions, float fix) {
    float angle = 6.2832/repetitions;
    float a = atan(p.y, p.x) + angle/2.;
    float r = length(p);
    float c = floor(a/angle);
    a = mod(a,angle) - (angle/2.)*fix;
    p = vec2(cos(a), sin(a))*r;

    return p;
}

float sdCylinder(vec3 p, vec3 a, vec3 b, float r) {
    vec3 ab = b-a;
    vec3 ap = p-a;
    
    float t = dot(ab, ap) / dot(ab, ab);
    //t = clamp(t, 0., 1.);
    
    vec3 c = a + t*ab;
    
    float x = length(p-c)-r;
    float y = (abs(t-.5)-.5)*length(ab);
    float e = length(max(vec2(x, y), 0.));
    float i = min(max(x, y), 0.);
    
    return e+i;
}

float sdBox(vec3 p, vec3 s) {
    p = abs(p)-s;
    return length(max(p, 0.))+min(max(p.x, max(p.y, p.z)), 0.);
}

float LineDist(vec2 a, vec2 b, vec2 p) {
    vec2 ab=b-a, ap=p-a;
    float h = dot(ab, ap)/dot(ab, ab);
    float d = length(ap - ab * h);
    float s = sign(ab.x * ap.y - ab.y * ap.x);
    return d*s;
}

float LineDist(float ax,float ay, float bx,float by, vec2 p) {
    return LineDist(vec2(ax, ay), vec2(bx, by), p);
}
float map01(float a, float b, float t) {
    return clamp((t-a)/(b-a), 0., 1.);
}
float map(float t, float a, float b, float c, float d) {
    return (d-c)*clamp((t-a)/(b-a), 0., 1.)+c;
}

vec2 RayLineDist(vec3 ro, vec3 rd, vec3 a, vec3 b) {
    
    b -= a;
    vec3 rdb = cross(rd,b);
    vec3 rop2 = a-ro;
    
    float t1 = dot( cross(rop2, b), rdb ); 
    float t2 = dot( cross(rop2, rd), rdb );
    
    return vec2(t1, t2) / dot(rdb, rdb);
}

float RayPlane(vec3 ro, vec3 rd, vec3 n, float d) {
    return (d-dot(ro, n)) / dot(rd, n);
}

float N21(vec2 p) {
    p = fract(p*vec2(123.34,456.23));
    p += dot(p, p+34.23);
    return fract(p.x*p.y);
    //return fract(sin(p.x*100.+p.y*6574.)*5647.);
}

float SmoothNoise(vec2 uv) {
    vec2 lv = fract(uv);
    vec2 id = floor(uv);
    
    lv = lv*lv*(3.-2.*lv);
    
    float bl = N21(id);
    float br = N21(id+vec2(1,0));
    float b = mix(bl, br, lv.x);
    
    float tl = N21(id+vec2(0,1));
    float tr = N21(id+vec2(1,1));
    float t = mix(tl, tr, lv.x);
    
    return mix(b, t, lv.y);
}

float SmoothNoise2(vec2 uv) {
    float c = SmoothNoise(uv*4.);
    
    // don't make octaves exactly twice as small
    // this way the pattern will look more random and repeat less
    c += SmoothNoise(uv*8.2)*.5;
    c += SmoothNoise(uv*16.7)*.25;
    c += SmoothNoise(uv*32.4)*.125;
    c += SmoothNoise(uv*64.5)*.0625;
    
    c /= 2.;
    
    return c;
}

float Tonemap_ACES(float x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec3 Tonemap_ACES(vec3 x) {
    return vec3(Tonemap_ACES(x.r),Tonemap_ACES(x.g),Tonemap_ACES(x.b));
}






// "Wait.. what? CyberTruck!" by Martijn Steinrucken aka BigWings/CountFrolic - 2019
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// Email: countfrolic@gmail.com
// Twitter: @The_ArtOfCode
// YouTube: youtube.com/TheArtOfCodeIsCool
//
// Music:
// https://soundcloud.com/weareallsynners/cyberpunk
//
// Tesla's insanely cool new pickup truck. I figured it'd be doable because of the angular look
// Still turned out to be A LOT of work. Code is quite messy as a result, which is quite usual
// for a version 1 of anything ;)
//
// Might do a run through of how this was made on The Art of Code if there is enough interest
//
// I have a quite convoluted way of rendering the interior behind the glass. 
// I'm raymarching to the glass and then spawning another raymarch loop for the interior.
// In hindsight that could have been done better, I think rendering everything without the glass
// and then rendering the glass on top would have been better. Oww well.
//
// If you are not fond of the disco effect, or if you want a better look at the car then
// I suggest lowering the BEAMS_PER_SECOND, pressing pause or activating MODEL_MODE
//

// Tweak these!
#define BEAMS_PER_SECOND 1.85/10.
#define GROUND_DISPLACEMENT
//#define MODEL_MODE


#define MAX_STEPS 300
#define MIN_DIST .5
#define MAX_DIST 60.
#define SURF_DIST .002

#define S(a, b, t) smoothstep(a, b, t)

#define MAT_BASE 0.
#define MAT_FENDERS 1.
#define MAT_RUBBER 2.
#define MAT_LIGHTS 3.
#define MAT_GLASS 4.
#define MAT_SHUTTERS 5.
#define MAT_GROUND 6.
#define MAT_CAB 6.



vec3 beamStart, beamEnd, beamCol;
float throughWindow;

vec2 sdCar(vec3 p) {
    float matId=MAT_BASE;
    p.x = sabs(p.x, .5);        // smooth abs to make front rounded
    
    vec2 P = p.yz;
    
    // body
    float d, w;
    
    float frontGlass = dot(P, vec2(0.9493, 0.3142))-1.506; // front
    d = frontGlass;
    
    float topGlass = dot(P, vec2(0.9938, -0.1110))-1.407;
    d = max(d, topGlass); 
    float back = dot(P, vec2(0.9887, -0.16))-1.424;
    d = max(d, back); // back
    
    float side1 = dot(p, vec3(0.9854, -0.1696, -0.0137))-0.580;
    d = max(d, side1); // side 1
    
    float side2 = dot(p, vec3(0.9661, 0.2583, 0.0037))-0.986;
    d = smin(d, side2, -.005);
    d = max(d, dot(P, vec2(-0.1578, -0.9875))-2.056); // rear
    d = max(d, dot(p, vec3(0.0952, -0.1171, 0.9885))-2.154);
    d = max(d, dot(p, vec3(0.5019, -0.1436, 0.8529))-2.051);
    d = max(d, dot(P, vec2(-0.9999, -0.0118))+0.2643); // bottom
    d = max(d, dot(p, vec3(0.0839, -0.4614, 0.8832))-1.770);
    d = max(d, dot(p, vec3(0.0247, -0.9653, 0.2599))-0.196);
    d = max(d, dot(P, vec2(-0.9486, -0.3163))-0.295);
    
    float body = d;
    float car = d;
    if((-frontGlass<car && p.z < 1.8-p.x*p.x*.16 && side2<-.01) ||
       (abs(-topGlass-car)<.01 && p.z>-.6 && p.z < .5 && side2<-.01)) 
        matId = MAT_GLASS;
    
    // bed shutters
    d = max(1.-p.y, max(p.x-.63, abs(p.z+1.44)-.73));
    if(d<-.02) matId = MAT_SHUTTERS;
    
    d = max(d, (-back-.01)-S(.5,1., sin(p.z*100.))*.0);
    
    car = max(car, -d);
    
    // bumper
    d = S(.03, .02, abs(p.y-.55))*.045;
    d -= S(.55, .52, p.y)*.05;
    d *= S(1.3, 1.5, abs(p.z));
    
    float rB = max(p.x-p.y*.15-.21, .45-p.y);
    float fB = max(p.x-.51, abs(.42-p.y)-.02);
    d *= S(.0,.01, mix(rB, fB, step(0.,p.z)));
       if(p.y<.58-step(abs(p.z), 1.3)) matId = MAT_FENDERS;
    
    // lights
    float lt = map01(.5, .8, p.x);
    float lights = map01(.02*(1.+lt), .01*(1.+lt), abs(p.y-(.82+lt*.03)));
    lights *= S(2.08, 2.3, p.z);
    d += lights*.05;
    lights = map01(.01, .0, side1+.0175);
    lights *= step(p.z, -2.17);
    lights *= map01(.01, .0, abs(p.y-1.04)-.025);
    d += lights*.03;
    
    if(d>0.&&matId==0.) matId = MAT_LIGHTS;
    
    if(car<.1) d*= .5;
    car += d;
    
    // step
    car += map(p.y+p.z*.022, .495, .325, 0., .05);//-S(.36, .34, p.y)*.1;
    d = sdBox(p-vec3(0, .32, 0), vec3(.72+p.z*.02, .03, 1.2));
    if(d<car) matId = MAT_FENDERS;
    car = min(car, d);
    
    // windows Holes
    
    d = w = dot(P, vec2(-0.9982, -0.0601))+1.0773;
    d = max(d, dot(P, vec2(0.1597, -0.9872))-0.795);
    d = max(d, dot(P, vec2(0.9931, -0.1177))-1.357);
    d = max(d, dot(P, vec2(0.9469, 0.3215))-1.459);
    //d = max(d, -.03-side2);
    float sideWindow = dot(p, vec3(-0.9687, -0.2481, 0.0106))+0.947;
    sideWindow += map01(0., 1., p.y-1.)*.05;
    if(d<-.005) matId = MAT_GLASS;
    
    d = max(d, sideWindow);
    car = max(car, -d);
    
    // panel lines
    if(car<.1) {
        d = abs(dot(p.yz, vec2(0.0393, 0.9992))+0.575);
        d = min(d, abs(dot(p.yz, vec2(0.0718, 0.9974))-0.3));
        d = min(d, abs(p.z-1.128));
        float panels = S(.005, .0025, d) * step(0., w) * step(.36, p.y);
        
        float handleY = dot(p.yz, vec2(-0.9988, -0.0493))+0.94;
        d = S(.02, .01, abs(handleY))*S(.01, .0, min(abs(p.z-.4)-.1, abs(p.z+.45)-.1));
        panels -= abs(d-.5)*.5;
        
        // charger
        d = S(.02, .01, abs(p.y-.81)-.04)*S(.01, .0, abs(p.z+1.75)-.12);
        panels += abs(d-.5)*.5;
        
        d = S(.005, .0, abs(side2+.015));
        d *= S(.03, .0, abs(frontGlass));
        panels += d;
        
        car += panels *.001;
    }
    
    // fenders
    //front
    d = dot(p, vec3(0.4614, 0.3362, 0.8210))-2.2130;
    d = max(d, dot(p, vec3(0.4561, 0.8893, 0.0347))-1.1552);
    d = max(d, dot(p, vec3(0.4792, 0.3783, -0.7920))+0.403);
    d = max(d, dot(p, vec3(0.4857, -0.0609, -0.8720))+0.6963);
    d = max(d, dot(p, vec3(0.4681, -0.4987, 0.7295))-1.545);
    d = max(d, .3-p.y);
    d = max(d, abs(p.x-.62-p.y*.15)-.07);
    if(d<car) matId = MAT_FENDERS;
    car = min(car, d);
    
    // back
    d = dot(p, vec3(0.4943, -0.0461, 0.8681))+0.4202;
    d = max(d, dot(p, vec3(0.4847, 0.4632, 0.7420))+0.0603);
    d = max(d, dot(p, vec3(0.4491, 0.8935, 0.0080))-1.081);
    d = max(d, dot(p, vec3(0.3819, 0.4822, -0.7885))-1.973);    
    d = max(d, min(.58-p.y, -1.5-p.z));
    d = max(d, .3-p.y);
    d = max(d, abs(side1+.01)-.08);
    if(d<car) matId = MAT_FENDERS;
    car = min(car, d);
    
    //if(car>.1) return vec2(car, matId);
    
    // wheel well
    // front
    d = p.z-2.0635;
    d = max(d, dot(p.yz, vec2(0.5285, 0.8489))-2.0260);
    d = max(d, dot(p.yz, vec2(0.9991, 0.0432))-0.8713);
    d = max(d, dot(p.yz, vec2(0.5258, -0.8506))+0.771);
    d = max(d, 1.194-p.z);
    d = max(d, .5-p.x);
    car = max(car, -d);
    if(d<car) matId = MAT_FENDERS;
    
    // back
    d = p.z+0.908;
    d = max(d, dot(p.yz, vec2(0.5906, 0.8070))+0.434);
    d = max(d, dot(p.yz, vec2(0.9998, 0.0176))-0.7843);
    d = max(d, dot(p, vec3(-0.0057, 0.5673, -0.8235))-1.7892);
    d = max(d, -p.z-1.7795);
       d = max(d, .5-p.x);//.65-p.x
    car = max(car, -d);
    if(d<car) matId = MAT_FENDERS;
    
   return vec2(car, matId);
}

vec2 sdWheel(vec3 p) {
    float matId=MAT_RUBBER;
    
    vec3 wp = p;
    float  w = sdCylinder(wp, vec3(-.1, 0,0), vec3(.1, 0,0), .32)-.03;
    float dist = length(wp.zy);
    
    if(dist>.3&&w<.05) {        // wheel detail
        float a = atan(wp.z, wp.y);
        float x = wp.x*20.;
        float tireTop = S(.29, .4, dist);
        float thread = S(-.5, -.3, sin(a*40.+x*x))*.01 * tireTop;
        
        thread *= S(.0, .007, abs(abs(wp.x)-.07+sin(a*20.)*.01));
        thread *= S(.005, .01, abs(wp.x+sin(a*20.)*.03));
        
        w -= thread*2.;
        
        float e = length(wp-vec3(2, .1, 0))-.5;
        w = min(w, e);
    }
    
    if(w>.1) return vec2(w, matId);
    
    wp *= .95;
    wp.yz = pModPolar(wp.yz, 7., 1.);
    float cap = max(p.x-.18, wp.y-.3);
    
    wp.z = abs(wp.z);
    
    float d = map01( .3, .23, wp.y);        // spoke bevel
    d *= map01(.04, .03, wp.z);            // spokes
    d *= map01(-.23, .23, wp.y)*.7;            // spoke gradient
    
    d = max(d, map01(.13, .0, wp.y)*1.5);    // center outside
    d = min(d, map01(.0, .07, wp.y));        // center inside
    d = max(d, .8*step(wp.y, .05));            // middle plateau
    
    d = max(d, .4*map01(.23, .22, dot(wp.zy, normalize(vec2(1., 2.)))));
    cap += (1.-d)*.07;
    cap = max(cap, .05-p.x);
    cap *= .8;
    if(cap<w) matId = MAT_FENDERS;
    
    w = min(w, cap);
    w += S(.3, .0, dist)*.025; // concavivy!
    
    return vec2(w, matId);
}

vec2 GetDist(vec3 p) {
    vec2 car = sdCar(p);
    vec3 wp = p-vec3(0,0,.14);
    wp.xz = abs(wp.xz);
    wp-=vec3(.7383, .365, 1.5);
    
    if(p.z>0.) wp.xz *= Rot(.3*sign(p.x));
    vec2 wheel = sdWheel(wp);
    
    float y = p.y;
    
    #ifdef GROUND_DISPLACEMENT
    float centerDist = dot(p.xz, p.xz);
    if(centerDist<100.&&p.y<.01) {
        y = SmoothNoise(p.xz*2.)+SmoothNoise(p.xz*5.)*.5+SmoothNoise(p.xz*23.)*.05;
        y += SmoothNoise(y*p.xz);
        
        float fade = S(100.,0.,centerDist);
        fade *= fade*fade;
        
        y = y*y*.03*fade;
        y *= S(.0, .6, dot(wp.xz,wp.xz));
    }
    y+=p.y;
    #endif
    
    if(min(y, min(car.x, wheel.x))==y)
        return vec2(y, MAT_GROUND);
    else 
        return car.x<wheel.x ? car : wheel;
}

vec3 RayMarch(vec3 ro, vec3 rd) {
    float dO=MIN_DIST;
    float dS;
    float matId=0.;
    
    for(int i=0; i<MAX_STEPS; i++) {
        vec3 p = ro + rd*dO;
        vec2 g = GetDist(p);
        dS = g.x;
        dO += dS;
        matId = g.y;
        if(dO>MAX_DIST || abs(dS)<SURF_DIST) break;
    }
    
    return vec3(dO, abs(dS), matId);
}

vec3 GetNormal(vec3 p) {
    float d = GetDist(p).x;
    vec2 e = vec2(1e-4, 0);
    
    vec3 n = d - vec3(
        GetDist(p-e.xyy).x,
        GetDist(p-e.yxy).x,
        GetDist(p-e.yyx).x);
    
    return normalize(n);
}


vec3 R(vec2 uv, vec3 p, vec3 l, float z) {
    vec3 f = normalize(l-p),
        r = normalize(cross(vec3(0,1,0), f)),
        u = cross(f,r),
        c = p+f*z,
        i = c + uv.x*r + uv.y*u,
        d = normalize(i-p);
    return d;
}



vec4 TubeInfo(vec3 ro, vec3 rd, vec3 a, vec3 b) {
    vec2 ts = RayLineDist(ro, rd, a, b);
    
    vec3 pr = ro+max(0., ts.x)*rd;            // closest point on ray
    vec3 pl = a+clamp(ts.y, 0., 1.)*(b-a);    // closest point on line
    float closestDist = length(pr-pl);        // distance between closest points
    //float distToCrossing = length(ro-pl);    // distance along ray to crossing
    
    return vec4(pl, closestDist);
}

float Intensity(float d, float w) {
    return exp(-(d*d)/w)/sqrt(w);
}


vec3 Ground(vec3 P) {
    vec2 p = P.xz;
    float d = 1.+max(0.,dot(p,p));
    float shadow = smoothstep(.5, 1.8, length(p-vec2(0, clamp(p.y, -1.5, 1.3))) );
    
    float albedo = SmoothNoise(p*4.+SmoothNoise(p*7.)*.66+SmoothNoise(p*13.)*.33);
    float specks = SmoothNoise(p*albedo*5.)+P.y;
    albedo -= specks*specks*specks;
    albedo /= 1.+max(0.,dot(p,p))*.5;
    
    albedo = (albedo+1.)/3.;
    vec3 col = vec3(albedo)*shadow;
    
    return col;
}

vec3 GroundRef(vec3 ro, vec3 rd) {
    vec2 p = ro.xz-rd.xz*(ro.y/rd.y);
    
    float d = 1.+max(0.,dot(p,p));
    float albedo = SmoothNoise(p*4.+SmoothNoise(p*7.)*.66+SmoothNoise(p*13.)*.33);
    
    albedo /= 1.+max(0.,dot(p,p))*.5;
    
    albedo = (albedo+1.)/3.;
    vec3 col = vec3(albedo)*S(.6, .0, rd.y);
    
    return col;
}

// Interior of the car
vec2 CabDist(vec3 p) {
    p.x = abs(p.x);
    
    float cab,d;
    
    float frontGlass = dot(p.yz, vec2(0.9493, 0.3142))-1.506; // front
    float topGlass = dot(p.yz, vec2(0.9938, -0.1110))-1.407;
    float windowBottom = dot(p.yz, vec2(-0.9982, -0.0601))+1.0773;
    
    float side1 = dot(p, vec3(0.9854, -0.1696, -0.0137))-0.580;
    float side2 = dot(p, vec3(0.9661, 0.2583, 0.0037))-0.986;
    float side = max(side1, side2);
    
    float w = .05;
    float hw = .5*w;
    
    float glass = max(frontGlass, topGlass);
    float glassShell = abs( glass+.025 ) -.025; 
    d = min(glassShell, max(abs(p.z-.17),glass+.1)-.05); // center column
    cab = max(d,abs(-side2-w)-w);
    
    // top bar
    d = max(abs(p.z-.43)-w, glassShell);
    d = max(d, side2);
    cab = min(cab, d);
    
    d = max(abs(-side1-w)-w, -windowBottom);    // side wall
    float walls = min(p.y-.3, p.z+.6);
    walls = min(d, walls);
    
    walls = max(walls, glass);
    d = max(walls,side);                // bottom
    cab = min(cab, d);
    
    // front seats
    float cup = cos(p.x*10.);
    vec3 seatPos = p-vec3(.35, .2+cup*.04, .8);
    d = sdBox(seatPos, vec3(.27, .25, .3)*.8)-.05;
    seatPos = p-vec3(.35, .75, .6);
    seatPos.z += S(.0, 1.2, p.y)*.4-cup*.05;
    vec3 seatScale = vec3(.27, .6, .03)*.8;
    seatScale.x *= 1.-S(.9, 1.1, p.y)*.6;
    seatScale.xz *= 1.-S(1.1, 1.3, p.y)*.7;
    
    d = min(d, sdBox(seatPos, seatScale)-.04);
    
    cab = min(cab, d);
    
    // dash
    d = sdBox(p-vec3(0,.5,1.7), vec3(2,.5,.3));
    d = min(d, sdBox(p-vec3(0,.89+(p.z-1.3)*.1,1.5), vec3(2,.07,.3))-.01);
    d = max(d, side1);
    cab = min(cab, d);
    
    // screen
    vec3 scrPos = p-vec3(0,.9,1.15);
    scrPos.yz *= Rot(-.4);
    d = sdBox(scrPos, vec3(.16,.1,-.005))-.02;
    cab = min(cab, d);
    // wheel
       
   
    return vec2(cab, MAT_CAB);
}

vec3 CabNormal(vec3 p) {
    float d = CabDist(p).x;
    vec2 e = vec2(1e-4, 0);
    
    vec3 n = d - vec3(
        CabDist(p-e.xyy).x,
        CabDist(p-e.yxy).x,
        CabDist(p-e.yyx).x);
    
    return normalize(n);
}

vec4 RenderCab(vec3 ro, vec3 rd) {
    vec4 info;
    
    float dO=MIN_DIST;
    float dS;
    float matId=0.;
    vec3 p;
    
    for(int i=0; i<MAX_STEPS; i++) {
        p = ro + rd*dO;
        vec2 g = CabDist(p);
        dS = g.x;
        dO += dS;
        matId = g.y;
        if(dO>MAX_DIST || abs(dS)<SURF_DIST) break;
    }
    
    vec3 col = vec3(0);
    if(abs(dS)<SURF_DIST) {
        vec3 n = CabNormal(p);
        info = vec4( n, 0. );
    } else
        info = vec4(0,0,0,.5);
    
    return info;
}

vec3 Material(vec3 ro, vec3 rd, vec3 p, vec3 n, vec3 d) {
    vec3 col = vec3(0);
    
    float dif = n.y;
    vec3 r = reflect(rd,n);
    
    vec4 nDif = TubeInfo(p, n, beamStart, beamEnd);
    vec4 nRef = TubeInfo(p, r, beamStart, beamEnd);

    float dist = length(nDif.xyz-p);
    float nDiffuse = 4.*max(0., dot(n, nDif.xyz/dist))/(dist*dist);
    nDiffuse *= S(50., 9., length(nDif.xyz-ro));

    vec3 wp = abs(p-vec3(0,0,.14))-vec3(.7383, .36, 1.5);
    float matId = d.z;
    
    if(matId==MAT_GROUND) {
        col = Ground(p);//ro, rd);
        float z = p.z-2.;
        float headLight = S(1.+z,-1.-z, abs(p.x)-p.z*.35);
        
        headLight -= headLight*n.z*2.; // bump map
        headLight /= 1.+z*z*.05;
        
        vec3 tlPos = p+vec3(0,0,3);
        col *= nDiffuse*beamCol+headLight*S(0., 3.,z)+
            .5/(1.+dot(tlPos,tlPos))*vec3(1.,.1,.1);
    } else if(matId==MAT_BASE || matId==MAT_GLASS || matId==MAT_SHUTTERS) { 
        vec3 ref = vec3(0.);//GetRef(p, r)*1.;

        ref += Intensity(nRef.w*(10.*(1.+matId)), length(p-nRef.xyz)+.25)*5.;
        ref *= beamCol;
        ref += GroundRef(p, r)*beamCol;

        vec3 P =p-r*(p.y/r.y);

        nDif = TubeInfo(P, vec3(0,1,0), beamStart, beamEnd);

        dist = length(nDif.xyz-P);
        nDiffuse = 4.*max(0., dot(n, nDif.xyz/dist))/(dist*dist);
        nDiffuse *= S(50., 9., length(nDif.xyz-ro));

        col += ref*nDiffuse;
        
        vec3 lighten = max(0., r.y*r.y*n.y)*beamCol;
        if(matId==MAT_BASE) {
            col += ref*max(.1,nDiffuse);
        
            //col *= 2.;
            col += lighten*.5;
        } else if(matId==MAT_SHUTTERS) {
            float seams = sin(p.z*150.)*.5+.5;
            col *= seams;
            col += lighten *.3*(S(.0, .1, seams)*.1+.9);  
        } else if(matId==MAT_GLASS) {
            vec4 cabInfo = RenderCab(ro, rd);
            col += cabInfo.y*.005;
            throughWindow = cabInfo.w;
        }        
    } else if(matId==MAT_LIGHTS) {
        if(p.z<0.)
            col.r += 1.;
        else
            col += 1.;
    } else if(matId==MAT_FENDERS) {
        float spec = Intensity(nRef.w*3., length(p-nRef.xyz)+.25);
        col += max(n.y*.05, spec)*(beamCol+.5);
        
    } else if(matId==MAT_RUBBER) {
        float shadow = S(.0, .1, abs(p.x)-.7);
        col += nDiffuse*shadow*beamCol*.2;
    }
    
    return col;      
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = (fragCoord-.5*iResolution.xy)/iResolution.y;
    vec4 m = iMouse/iResolution.xyxy;
    if(m.x<.05) m.xy = vec2(.7,.45);
    float t = iTime;
    vec3 col = vec3(0);
    
    vec3 ro = vec3(0, 4, -5)*.7;
    ro.yz *= Rot(-m.y*3.14+1.);
    ro.xz *= Rot(-m.x*6.2831+t*.3+1.);
    ro.y = max(ro.y, .1);
    vec3 lookat = vec3(0,.5,0);
    //lookat = vec3(.7,.5,1.5);
    #ifdef MODEL_MODE
    vec3 rd = R(uv, ro, lookat, 2.);
    #else
    vec3 rd = R(uv, ro, lookat, 1.);
    #endif
    
    vec3 d = RayMarch(ro, rd);
    throughWindow = 0.;        // will be set to >0 if we are looking all the way through the car
   
    // neon tubes
    //float climax = 1.+step(35.,float(iChannelTime[0]))*5.*step(float(iChannelTime[0]), 52.);
    float climax = 1.+step(35.,float(iFrame%83))*5.*step(float(iFrame%31), 52.); //kludge: missing shadertoy's audio iChannelTime
    float ft = floor(t*BEAMS_PER_SECOND*climax)*BEAMS_PER_SECOND*climax;
       beamStart = vec3(-40,3.,0)+sin(ft*vec3(.234,.453,.486))*vec3(5,2,5)*.5;
    beamEnd = vec3(40, 3.,0)+sin(ft*vec3(.345,.538,.863))*vec3(5,2,5)*.5;
    mat2 rot = Rot(ft*45.4532);
    beamStart.xz *=rot;
    beamEnd.xz*=rot;
    
      beamCol = sin(ft*vec3(.234,.453,.486))*.5+.5;
    beamCol *= beamCol;
    beamCol = normalize(beamCol);
    
    vec2 ts;
    vec3 a, b, s;
    float nd;
    vec3 p = ro + rd * d.x;
    
    if(d.y<SURF_DIST) {
        vec3 n = GetNormal(p);
        
        #ifdef MODEL_MODE
        float matId = d.z;
        col = vec3(1);//n*.5+.5;

        if(matId==MAT_BASE)
            col *= vec3(1,0,0);
        else if(matId==MAT_FENDERS)
            col *= vec3(0,1,0);
        else if(matId==MAT_LIGHTS)
            col *= vec3(0,0,1);
        else if(matId==MAT_GLASS) {
            vec4 cabInfo = RenderCab(ro, rd);
            col = cabInfo.xyz/3.;
            throughWindow = cabInfo.w;
        }else if(matId==MAT_RUBBER)
            col *= vec3(1,1,0);
        else if(matId==MAT_SHUTTERS)
            col *= vec3(0,1,1);
        else if(matId==MAT_GROUND)
            col *= .2;
        #else
        col = Material(ro, rd, p, n, d);
        #endif
    } else if(rd.y>0.){
        col += rd.y*rd.y*(beamCol);
    }
    
    #ifndef MODEL_MODE
    if(rd.y<0.) {
        float groundDist = length(rd*(ro.y/rd.y));
        if(groundDist<d.x){// || groundDist>MAX_DIST) {
            vec3 groundPos = ro+groundDist*rd;
        }
    }
    
    ts = RayLineDist(ro, rd, beamStart, beamEnd);
    a = ro+max(0., ts.x)*rd;
    b = beamStart+clamp(ts.y, 0., 1.)*(beamEnd-beamStart);
    nd = length(a-b);
    float dist = length(ro-b);
    
    float beam = (.005/dot(nd,nd))*S(50., 9., dist);
    beam *= max(throughWindow, S(0., 1., d.x-length(ro-b)));
    col += beam*beamCol;
    
    // headlights
    float brightness = .02;
    float z = 2.22;
    float bias = 1.;
    float offs = .0;
    vec4 h;
    
    h = TubeInfo(ro, rd, vec3(.56, .82, z), vec3(.74,.85,z-.1));
    float headlight = max(0., dot(vec2(.3, .8), -rd.xz)*bias+offs)*brightness/h.w;
    h = TubeInfo(ro, rd, vec3(-.56, .82, z), vec3(-.74,.85,z-.1));
    headlight += max(0., dot(vec2(-.3, .8), -rd.xz)*bias+offs)*brightness/h.w;
    
    // middle
    brightness *= .66;
    h = TubeInfo(ro, rd, vec3(-.52, .82, z), vec3(.52,.82,z));
    headlight += max(0., -rd.z)*brightness/h.w;
    
    // top
   // h = TubeInfo(ro, rd, vec3(-.52, 1.43, .5), vec3(.52,1.43,.5));
   // headlight += max(0., -rd.z)*.005/h.w;
    
    
    col += headlight*vec3(.8, .8, 1);
    
    // rear light
    h = TubeInfo(ro, rd, vec3(-.7, 1.05, -2.25), vec3(.7,1.05,-2.25));
    col += vec3(1., .1, .1)*max(0., rd.z*rd.z*rd.z)*.04/h.w;
    
    
    //col *= 3.;
    
    //if(uv.x>0.)col =  col*3.;//filmic_reinhard(col); else
    col = Tonemap_ACES(col*4.);
    col *= 1.-dot(uv,uv)*.5;
    
    #endif
    //col = pow(col,vec3(1./2.2));
   fragColor = vec4(col,1.0);
}
