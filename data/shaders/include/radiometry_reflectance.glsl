#ifndef MF_RADIOMETRY_REFLECTANCE_GLSL
#define MF_RADIOMETRY_REFLECTANCE_GLSL

/** Mirrors the evaluations in Kinesis/Radiometry/Reflectance.hpp. */

#include "radiometry_fresnel.glsl"
#include "radiometry_microfacet.glsl"

/** What a surface point does with arriving radiance, in the metallic and roughness model. */
struct SurfaceResponse {
    vec3 base_color;
    float roughness;
    float metallic;
    float diffuse_roughness;
    float dielectric_f0;
};

const float k_fon_a = 0.5 - 2.0 / (3.0 * MF_PI);
const float k_fon_average = 2.0 / 3.0 - 28.0 / (15.0 * MF_PI);

float fon_albedo(float cos_theta, float roughness)
{
    float m = 1.0 - cos_theta;
    float g_over_pi = m * (0.0571085289 + m * (0.491881867 + m * (-0.332181442 + m * 0.0714429953)));
    return (1.0 + roughness * g_over_pi) / (1.0 + k_fon_a * roughness);
}

/** Energy preserving Oren-Nayar (Portsmouth, Kutz and Hill), without the cosine. */
vec3 oren_nayar_energy_preserving(vec3 albedo, float roughness, vec3 normal, vec3 view, vec3 source)
{
    float cos_source = dot(normal, source);
    float cos_view = dot(normal, view);
    if (!(cos_source > 0.0) || !(cos_view > 0.0)) {
        return vec3(0.0);
    }

    float s = dot(source, view) - cos_source * cos_view;
    float s_over_t = s > 0.0 ? s / max(max(cos_source, cos_view), 1e-6) : s;

    float a = 1.0 / (1.0 + k_fon_a * roughness);
    vec3 single_lobe = albedo * (a * (1.0 + roughness * s_over_t) / MF_PI);

    float average = a * (1.0 + k_fon_average * roughness);
    vec3 multi_albedo = albedo * albedo * average / (vec3(1.0) - albedo * (1.0 - average));
    const float least = 1e-7;
    vec3 multiple_lobe = multi_albedo
        * (max(least, 1.0 - fon_albedo(cos_view, roughness))
            * max(least, 1.0 - fon_albedo(cos_source, roughness))
            / (max(least, 1.0 - average) * MF_PI));

    return single_lobe + multiple_lobe;
}

/**
 * Reflectance for one pair of unit directions, without the cosine of the source.
 * specular_albedo is the specular lobe's directional albedo at unit Fresnel; one
 * applies no multiple scattering compensation.
 */
vec3 surface_reflectance(SurfaceResponse surface, vec3 normal, vec3 view, vec3 source, float specular_albedo)
{
    float cos_view = dot(normal, view);
    float cos_source = dot(normal, source);
    if (!(cos_view > 0.0) || !(cos_source > 0.0)) {
        return vec3(0.0);
    }

    vec3 halfway = normalize(view + source);
    float cos_difference = max(dot(view, halfway), 0.0);
    float alpha = roughness_to_alpha(surface.roughness);

    vec3 f0 = mix(vec3(surface.dielectric_f0), surface.base_color, surface.metallic);
    vec3 specular = schlick_fresnel(f0, cos_difference)
        * (ggx_density(dot(normal, halfway), alpha) * ggx_visibility(cos_view, cos_source, alpha))
        * multiple_scattering_compensation(f0, specular_albedo);

    vec3 boundary = vec3(surface.dielectric_f0);
    vec3 entering = (vec3(1.0) - schlick_fresnel(boundary, cos_view))
        * (vec3(1.0) - schlick_fresnel(boundary, cos_source))
        / (vec3(1.0) - schlick_average(boundary));
    vec3 diffuse = entering * (1.0 - surface.metallic)
        * oren_nayar_energy_preserving(surface.base_color, surface.diffuse_roughness, normal, view, source);

    return diffuse + specular;
}

#endif
