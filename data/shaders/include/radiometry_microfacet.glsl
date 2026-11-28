#ifndef MF_RADIOMETRY_MICROFACET_GLSL
#define MF_RADIOMETRY_MICROFACET_GLSL

/** Mirrors the evaluations in Kinesis/Radiometry/Microfacet.hpp. */

#ifndef MF_PI
#define MF_PI 3.14159265358979323846
#endif

/** GGX width from a perceptual roughness. */
float roughness_to_alpha(float roughness)
{
    return max(roughness * roughness, 1e-3);
}

/** GGX distribution of microfacet normals. */
float ggx_density(float cos_half, float alpha)
{
    if (!(cos_half > 0.0)) {
        return 0.0;
    }
    float a2 = alpha * alpha;
    float c2 = min(cos_half * cos_half, 1.0);
    float d = c2 * a2 + (1.0 - c2);
    return a2 / (MF_PI * d * d);
}

/** Smith masking of GGX seen from one direction. */
float ggx_masking(float cos_theta, float alpha)
{
    if (!(cos_theta > 0.0)) {
        return 0.0;
    }
    float a2 = alpha * alpha;
    return 2.0 * cos_theta / (cos_theta + sqrt(a2 + (1.0 - a2) * cos_theta * cos_theta));
}

/** Height correlated Smith masking and shadowing over 4 cos_view cos_source. */
float ggx_visibility(float cos_view, float cos_source, float alpha)
{
    float a2 = alpha * alpha;
    float view = cos_source * sqrt(cos_view * cos_view * (1.0 - a2) + a2);
    float source = cos_view * sqrt(cos_source * cos_source * (1.0 - a2) + a2);
    float sum = view + source;
    return sum > 0.0 ? 0.5 / sum : 0.0;
}

/** Turquin's compensation; albedo one leaves the lobe unchanged. */
vec3 multiple_scattering_compensation(vec3 f0, float albedo)
{
    return vec3(1.0) + f0 * (1.0 / max(albedo, 1e-4) - 1.0);
}

#endif
