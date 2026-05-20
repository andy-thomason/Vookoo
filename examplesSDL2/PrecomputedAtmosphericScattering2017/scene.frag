#version 450
#extension GL_GOOGLE_include_directive : require

// Vulkan/VKU/SDL2 port of the Bruneton 2017 precomputed atmospheric scattering demo.
//
// Original work:
//   Eric Bruneton, "A Scalable and Production Ready Sky and Atmosphere Rendering
//   Technique", Eurographics Symposium on Rendering 2017.
//   https://github.com/ebruneton/precomputed_atmospheric_scattering
//   Copyright (c) 2017 Eric Bruneton. BSD 3-Clause License.
//
//   Eric Bruneton, Fabrice Neyret, "Precomputed Atmospheric Scattering",
//   Eurographics Symposium on Rendering 2008.
//   Copyright (c) 2008 INRIA. BSD 3-Clause License.
//
// This file: scene fragment shader. Includes definitions.glsl and functions.glsl
// from the original demo unchanged. AtmosphereParameters read from a std140 UBO
// (binding 5) and forwarded to each parameterised function call.

#define IN(x) const in x
#define OUT(x) out x
#define TEMPLATE(x)
#define TEMPLATE_ARGUMENT(x)
#define assert(x)

const int TRANSMITTANCE_TEXTURE_WIDTH  = 256;
const int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
const int SCATTERING_TEXTURE_R_SIZE    = 32;
const int SCATTERING_TEXTURE_MU_SIZE   = 128;
const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
const int SCATTERING_TEXTURE_NU_SIZE   = 8;
const int IRRADIANCE_TEXTURE_WIDTH     = 64;
const int IRRADIANCE_TEXTURE_HEIGHT    = 16;
#define COMBINED_SCATTERING_TEXTURES

#include "definitions.glsl"
#include "functions.glsl"

layout(set=0, binding=0) uniform sampler2D transmittance_texture;
layout(set=0, binding=1) uniform sampler3D scattering_texture;
layout(set=0, binding=2) uniform sampler3D single_mie_scattering_texture;
layout(set=0, binding=3) uniform sampler2D irradiance_texture;

layout(set=0, binding=4, std140) uniform SceneUBO {
    mat4 model_from_view;
    mat4 view_from_clip;
    vec4 camera_exposure;
    vec4 white_point;
    vec4 earth_center;
    vec4 sun_direction;
    vec2 sun_size;
} u;

layout(set=0, binding=5, std140) uniform AtmosphereUBO {
    AtmosphereParameters atmosphere;
} atmo;

layout(location=0) in  vec3 inRay;
layout(location=0) out vec4 outColor;

RadianceSpectrum GetSolarRadiance() {
    return atmo.atmosphere.solar_irradiance /
        (PI * atmo.atmosphere.sun_angular_radius * atmo.atmosphere.sun_angular_radius);
}

RadianceSpectrum GetSkyRadiance(
        Position camera, Direction view_ray, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
    return GetSkyRadiance(atmo.atmosphere, transmittance_texture,
        scattering_texture, single_mie_scattering_texture,
        camera, view_ray, shadow_length, sun_direction, transmittance);
}

RadianceSpectrum GetSkyRadianceToPoint(
        Position camera, Position point, Length shadow_length,
        Direction sun_direction, out DimensionlessSpectrum transmittance) {
    return GetSkyRadianceToPoint(atmo.atmosphere, transmittance_texture,
        scattering_texture, single_mie_scattering_texture,
        camera, point, shadow_length, sun_direction, transmittance);
}

IrradianceSpectrum GetSunAndSkyIrradiance(
        Position p, Direction normal, Direction sun_direction,
        out IrradianceSpectrum sky_irradiance) {
    return GetSunAndSkyIrradiance(atmo.atmosphere, transmittance_texture,
        irradiance_texture, p, normal, sun_direction, sky_irradiance);
}

const float kLengthUnitInMeters = 1000.0;
const vec3  kSphereCenter = vec3(0.0, 0.0, 1000.0) / kLengthUnitInMeters;
const float kSphereRadius = 1000.0 / kLengthUnitInMeters;
const vec3  kSphereAlbedo = vec3(0.8);
const vec3  kGroundAlbedo = vec3(0.0, 0.0, 0.04);

float GetSunVisibility(vec3 point, vec3 sun_dir) {
    vec3  p = point - kSphereCenter;
    float p_dot_v = dot(p, sun_dir);
    float p_dot_p = dot(p, p);
    float ray_sphere_center_sq = p_dot_p - p_dot_v * p_dot_v;
    float disc = kSphereRadius * kSphereRadius - ray_sphere_center_sq;
    if (disc >= 0.0) {
        float d = -p_dot_v - sqrt(disc);
        if (d > 0.0) {
            float dist  = kSphereRadius - sqrt(ray_sphere_center_sq);
            float angle = -dist / p_dot_v;
            return smoothstep(1.0, 0.0, angle / u.sun_size.x);
        }
    }
    return 1.0;
}

float GetSkyVisibility(vec3 point) {
    vec3  p = point - kSphereCenter;
    float r = dot(p, p);
    return 1.0 + p.z / sqrt(r) * kSphereRadius * kSphereRadius / r;
}

void GetSphereShadowInOut(vec3 view_dir, vec3 sun_dir, out float d_in, out float d_out) {
    vec3  pos          = u.camera_exposure.xyz - kSphereCenter;
    float pos_dot_sun  = dot(pos, sun_dir);
    float view_dot_sun = dot(view_dir, sun_dir);
    float k = u.sun_size.x;
    float l = 1.0 + k * k;
    float a = 1.0 - l * view_dot_sun * view_dot_sun;
    float b = dot(pos, view_dir) - l * pos_dot_sun * view_dot_sun
              - k * kSphereRadius * view_dot_sun;
    float c = dot(pos, pos) - l * pos_dot_sun * pos_dot_sun
              - 2.0 * k * kSphereRadius * pos_dot_sun - kSphereRadius * kSphereRadius;
    float disc = b * b - a * c;
    if (disc > 0.0) {
        d_in  = max(0.0, (-b - sqrt(disc)) / a);
        d_out = (-b + sqrt(disc)) / a;
        float d_base = -pos_dot_sun / view_dot_sun;
        float d_apex = -(pos_dot_sun + kSphereRadius / k) / view_dot_sun;
        if (view_dot_sun > 0.0) {
            d_in  = max(d_in, d_apex);
            d_out = a > 0.0 ? min(d_out, d_base) : d_base;
        } else {
            d_in  = a > 0.0 ? max(d_in, d_base) : d_base;
            d_out = min(d_out, d_apex);
        }
    } else {
        d_in = d_out = 0.0;
    }
}

void main() {
    vec3  camera        = u.camera_exposure.xyz;
    float exposure      = u.camera_exposure.w;
    vec3  white_point   = u.white_point.xyz;
    vec3  earth_center  = u.earth_center.xyz;
    vec3  sun_direction = u.sun_direction.xyz;

    vec3  view_direction = normalize(inRay);
    float frag_ang = length(dFdx(inRay) + dFdy(inRay)) / length(inRay);

    float shadow_in, shadow_out;
    GetSphereShadowInOut(view_direction, sun_direction, shadow_in, shadow_out);
    float lightshaft_fade = smoothstep(0.02, 0.04,
        dot(normalize(camera - earth_center), sun_direction));

    // Sphere
    vec3  p_sphere    = camera - kSphereCenter;
    float pdotv       = dot(p_sphere, view_direction);
    float pdotp       = dot(p_sphere, p_sphere);
    float rsq_dist    = pdotp - pdotv * pdotv;
    float disc_sphere = kSphereRadius * kSphereRadius - rsq_dist;

    float sphere_alpha    = 0.0;
    vec3  sphere_radiance = vec3(0.0);
    if (disc_sphere >= 0.0) {
        float dist = -pdotv - sqrt(disc_sphere);
        if (dist > 0.0) {
            float ang = -(kSphereRadius - sqrt(rsq_dist)) / pdotv;
            sphere_alpha = min(ang / frag_ang, 1.0);

            vec3  point  = camera + view_direction * dist;
            vec3  normal = normalize(point - kSphereCenter);
            vec3  sky_irr;
            vec3  sun_irr = GetSunAndSkyIrradiance(
                point - earth_center, normal, sun_direction, sky_irr);
            sphere_radiance = kSphereAlbedo * (1.0 / PI) * (sun_irr + sky_irr);

            float sl = max(0.0, min(shadow_out, dist) - shadow_in) * lightshaft_fade;
            vec3  transmittance;
            vec3  in_scatter = GetSkyRadianceToPoint(
                camera - earth_center, point - earth_center,
                sl, sun_direction, transmittance);
            sphere_radiance = sphere_radiance * transmittance + in_scatter;
        }
    }

    // Ground
    vec3  p_earth    = camera - earth_center;
    float pedotv     = dot(p_earth, view_direction);
    float pedotp     = dot(p_earth, p_earth);
    // earth_center = (0,0,-R) so earth_center.z*earth_center.z = R^2
    float disc_earth = earth_center.z * earth_center.z - (pedotp - pedotv * pedotv);

    float ground_alpha    = 0.0;
    vec3  ground_radiance = vec3(0.0);
    if (disc_earth >= 0.0) {
        float dist = -pedotv - sqrt(disc_earth);
        if (dist > 0.0) {
            vec3  point  = camera + view_direction * dist;
            vec3  normal = normalize(point - earth_center);
            vec3  sky_irr;
            vec3  sun_irr = GetSunAndSkyIrradiance(
                point - earth_center, normal, sun_direction, sky_irr);
            ground_radiance = kGroundAlbedo * (1.0 / PI) *
                (sun_irr * GetSunVisibility(point, sun_direction) +
                 sky_irr * GetSkyVisibility(point));

            float sl = max(0.0, min(shadow_out, dist) - shadow_in) * lightshaft_fade;
            vec3  transmittance;
            vec3  in_scatter = GetSkyRadianceToPoint(
                camera - earth_center, point - earth_center,
                sl, sun_direction, transmittance);
            ground_radiance = ground_radiance * transmittance + in_scatter;
            ground_alpha = 1.0;
        }
    }

    // Sky
    float shadow_length = max(0.0, shadow_out - shadow_in) * lightshaft_fade;
    vec3  transmittance;
    vec3  radiance = GetSkyRadiance(
        camera - earth_center, view_direction,
        shadow_length, sun_direction, transmittance);
    if (dot(view_direction, sun_direction) > u.sun_size.y) {
        radiance += transmittance * GetSolarRadiance();
    }
    radiance = mix(radiance, ground_radiance, ground_alpha);
    radiance = mix(radiance, sphere_radiance, sphere_alpha);

    outColor.rgb = pow(vec3(1.0) - exp(-radiance / white_point * exposure),
                       vec3(1.0 / 2.2));
    outColor.a   = 1.0;
}
