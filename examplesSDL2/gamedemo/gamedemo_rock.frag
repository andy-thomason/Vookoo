#version 450

layout(location=0) in  vec2  v_local;
layout(location=1) in  float v_angle;
layout(location=2) in  float v_seed;

layout(location=0) out vec4  out_color;

// ---- Classic 3D Perlin noise — Stefan Gustavson (MIT), from examples/perlinNoise ----
vec3 mod289(vec3 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec3 fade(vec3 t) { return t*t*t*(t*(t*6.0-15.0)+10.0); }

float cnoise(vec3 P) {
    vec3 Pi0 = floor(P), Pi1 = Pi0 + 1.0;
    Pi0 = mod289(Pi0); Pi1 = mod289(Pi1);
    vec3 Pf0 = fract(P), Pf1 = Pf0 - 1.0;
    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.yy, Pi1.yy);
    vec4 ixy  = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + Pi0.zzzz);
    vec4 ixy1 = permute(ixy + Pi1.zzzz);
    vec4 gx0 = ixy0*(1.0/7.0), gx1 = ixy1*(1.0/7.0);
    vec4 gy0 = fract(floor(gx0)*(1.0/7.0)) - 0.5;
    vec4 gy1 = fract(floor(gx1)*(1.0/7.0)) - 0.5;
    gx0 = fract(gx0); gx1 = fract(gx1);
    vec4 gz0 = 0.5 - abs(gx0) - abs(gy0), sz0 = step(gz0, vec4(0.0));
    vec4 gz1 = 0.5 - abs(gx1) - abs(gy1), sz1 = step(gz1, vec4(0.0));
    gx0 -= sz0*(step(0.0,gx0)-0.5); gy0 -= sz0*(step(0.0,gy0)-0.5);
    gx1 -= sz1*(step(0.0,gx1)-0.5); gy1 -= sz1*(step(0.0,gy1)-0.5);
    vec3 g000=vec3(gx0.x,gy0.x,gz0.x), g100=vec3(gx0.y,gy0.y,gz0.y);
    vec3 g010=vec3(gx0.z,gy0.z,gz0.z), g110=vec3(gx0.w,gy0.w,gz0.w);
    vec3 g001=vec3(gx1.x,gy1.x,gz1.x), g101=vec3(gx1.y,gy1.y,gz1.y);
    vec3 g011=vec3(gx1.z,gy1.z,gz1.z), g111=vec3(gx1.w,gy1.w,gz1.w);
    vec4 norm0 = taylorInvSqrt(vec4(dot(g000,g000),dot(g010,g010),dot(g100,g100),dot(g110,g110)));
    g000*=norm0.x; g010*=norm0.y; g100*=norm0.z; g110*=norm0.w;
    vec4 norm1 = taylorInvSqrt(vec4(dot(g001,g001),dot(g011,g011),dot(g101,g101),dot(g111,g111)));
    g001*=norm1.x; g011*=norm1.y; g101*=norm1.z; g111*=norm1.w;
    float n000=dot(g000,Pf0),          n100=dot(g100,vec3(Pf1.x,Pf0.yz));
    float n010=dot(g010,vec3(Pf0.x,Pf1.y,Pf0.z)), n110=dot(g110,vec3(Pf1.xy,Pf0.z));
    float n001=dot(g001,vec3(Pf0.xy,Pf1.z)),       n101=dot(g101,vec3(Pf1.x,Pf0.y,Pf1.z));
    float n011=dot(g011,vec3(Pf0.x,Pf1.yz)),       n111=dot(g111,Pf1);
    vec3 fxyz = fade(Pf0);
    vec4 nz = mix(vec4(n000,n100,n010,n110), vec4(n001,n101,n011,n111), fxyz.z);
    vec2 nyz = mix(nz.xy, nz.zw, fxyz.y);
    return 2.2 * mix(nyz.x, nyz.y, fxyz.x);
}

// Turbulence: sum of abs(cnoise) at increasing frequencies — gives bumpy surface.
float turbulence(vec3 p) {
    float t = 0.0, a = 1.0;
    for (int i = 0; i < 4; ++i) {
        t += a * abs(cnoise(p));
        p *= 2.2; a *= 0.45;
    }
    return t;
}
// -------------------------------------------------------------------------------------

void main() {
    float r = length(v_local);
    if (r > 1.05) discard;  // quad corners, definitely outside rock

    // Transform to rock body frame so shape and texture rotate with the rock.
    float c = cos(v_angle), s = sin(v_angle);
    vec2 body = vec2(c*v_local.x + s*v_local.y, -s*v_local.x + c*v_local.y);

    // Noisy boundary: sample noise on the unit circle in body frame.
    vec2 bdir = r > 0.001 ? body / r : vec2(1.0, 0.0);
    float rock_r = 0.80 + cnoise(vec3(bdir * 2.8, v_seed)) * 0.18;
    if (r > rock_r) discard;

    // Bumpy surface texture via turbulence in body frame.
    float surf = turbulence(vec3(body * 2.5, v_seed + 3.7));

    // Lava palette matching examples/18 explosion texture: dark red -> orange -> yellow.
    float t = clamp(surf * 0.5, 0.0, 1.0);
    vec3 col_a = vec3(0.08, 0.01, 0.00);
    vec3 col_b = vec3(0.38, 0.06, 0.00);
    vec3 col_c = vec3(0.68, 0.26, 0.01);
    vec3 col_d = vec3(0.82, 0.58, 0.07);
    vec3 color;
    if      (t < 0.33) color = mix(col_a, col_b, t * 3.03);
    else if (t < 0.67) color = mix(col_b, col_c, (t - 0.33) * 2.94);
    else               color = mix(col_c, col_d, (t - 0.67) * 3.03);

    // Darker toward the edge — cooler crust.
    color *= 1.0 - smoothstep(rock_r * 0.60, rock_r, r) * 0.65;

    // Glowing orange rim at the boundary.
    float rim = smoothstep(rock_r - 0.08, rock_r - 0.01, r);
    color += vec3(1.0, 0.55, 0.05) * rim * 0.60;

    out_color = vec4(color, 1.0);
}
