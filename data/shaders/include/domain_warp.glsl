#ifndef DOMAIN_WARP_GLSL
#define DOMAIN_WARP_GLSL

#ifndef NOISE_GLSL
#error "domain_warp.glsl requires noise.glsl — include it first"
#endif

// ---------------------------------------------------------------------------
// Domain warping
// Based on: Inigo Quilez, https://iquilezles.org/articles/warp/
//
// Evaluates a vector offset field at p using three independent noise calls
// with phase offsets, then returns the warped position. Feed the result
// into any SDF or noise function instead of p directly.
//
// Single-pass warp:   p' = p + strength * warp_vec(p, t)
// Two-pass warp:      p'' = p' + strength * warp_vec(p', t)  (stronger curl)
//
// Example — fluid-like sphere:
//   vec3 wp = domain_warp(p, 1.5, 0.4, pc.time);
//   float d = sdf_sphere(wp, vec3(0.0), 1.0);
// ---------------------------------------------------------------------------

/// @brief Compute a 3D warp offset vector at p.
/// @param p         Evaluation point.
/// @param frequency Spatial frequency of the warp field.
/// @param t         Time offset for animation.
/// @return          Warp offset vector, each component in [-1, 1].
vec3 warp_vec(vec3 p, float frequency, float t)
{
    vec3 q = p * frequency;
    return vec3(
        noise(q + vec3(0.0, 0.0, t)),
        noise(q + vec3(5.2, 1.3, t)),
        noise(q + vec3(9.7, 2.8, t)));
}

/// @brief Single-pass domain warp.
/// @param p         Evaluation point.
/// @param frequency Spatial frequency of the warp field.
/// @param strength  Warp displacement magnitude.
/// @param t         Time offset for animation.
/// @return          Warped position.
vec3 domain_warp(vec3 p, float frequency, float strength, float t)
{
    return p + warp_vec(p, frequency, t) * strength;
}

/// @brief Two-pass domain warp — stronger curl and folding.
/// @param p         Evaluation point.
/// @param frequency Spatial frequency of the warp field.
/// @param strength  Warp displacement magnitude per pass.
/// @param t         Time offset for animation.
/// @return          Twice-warped position.
vec3 domain_warp2(vec3 p, float frequency, float strength, float t)
{
    vec3 p1 = domain_warp(p, frequency, strength, t);
    return domain_warp(p1, frequency, strength, t);
}

#endif // DOMAIN_WARP_GLSL
