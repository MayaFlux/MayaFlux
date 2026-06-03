#ifndef NOISE_GLSL
#define NOISE_GLSL

// ---------------------------------------------------------------------------
// Hash (hardware-stable, no sin)
// Based on: https://www.shadertoy.com/view/4djSRW  (Dave Hoskins, MIT)
// Avoids fract(sin(x)*large) which produces broken results on some GPUs due
// to float precision differences between vendors.
// ---------------------------------------------------------------------------

vec3 hash33(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

float hash13(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

// ---------------------------------------------------------------------------
// 3D gradient noise
// Based on: Inigo Quilez, https://www.shadertoy.com/view/Xsl3Dl  (MIT)
// Returns value in [-1, 1].
// ---------------------------------------------------------------------------

float noise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(mix(dot(hash33(i + vec3(0, 0, 0)), f - vec3(0, 0, 0)),
                dot(hash33(i + vec3(1, 0, 0)), f - vec3(1, 0, 0)), u.x),
            mix(dot(hash33(i + vec3(0, 1, 0)), f - vec3(0, 1, 0)),
                dot(hash33(i + vec3(1, 1, 0)), f - vec3(1, 1, 0)), u.x), u.y),
        mix(mix(dot(hash33(i + vec3(0, 0, 1)), f - vec3(0, 0, 1)),
                dot(hash33(i + vec3(1, 0, 1)), f - vec3(1, 0, 1)), u.x),
            mix(dot(hash33(i + vec3(0, 1, 1)), f - vec3(0, 1, 1)),
                dot(hash33(i + vec3(1, 1, 1)), f - vec3(1, 1, 1)), u.x), u.y),
        u.z);
}

// ---------------------------------------------------------------------------
// Fractal Brownian Motion
// Octave rotation matrix reduces axis-aligned artifacts between octaves.
// Based on: Inigo Quilez, https://iquilezles.org/articles/fbm/
// ---------------------------------------------------------------------------

const mat3 k_fbm_rot = mat3(
        0.00, 0.80, 0.60,
        -0.80, 0.36, -0.48,
        -0.60, -0.48, 0.64);

/// @brief FBM with octave rotation. Returns value in roughly [-1, 1].
/// @param p     Evaluation point.
/// @param octaves  Number of octaves (4-8 is typical).
/// @param lacunarity  Frequency multiplier per octave (default 2.0).
/// @param gain      Amplitude multiplier per octave (default 0.5).
float fbm(vec3 p, int octaves, float lacunarity, float gain)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * noise(p);
        p = k_fbm_rot * p * lacunarity;
        amplitude *= gain;
    }
    return value;
}

/// @brief FBM with defaults: 5 octaves, lacunarity 2.0, gain 0.5.
float fbm(vec3 p) {
    return fbm(p, 5, 2.0, 0.5);
}

// ---------------------------------------------------------------------------
// Turbulence: abs(noise) summed across octaves.
//
// Returns values in [0, ~1]. Mean output is above 0.5 because abs() folds
// the negative half up. Do NOT use as a standalone SDF — it has no reliable
// zero crossing. Use as a displacement on top of a base signed distance:
//
//   float d = sdf_sphere(p, center, r) + turbulence(p * freq + t) * strength;
//
// That preserves the zero crossing from the base SDF while deforming the
// surface with turbulent detail.
// ---------------------------------------------------------------------------

float turbulence(vec3 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < octaves; ++i) {
        value += amplitude * abs(noise(p));
        p = k_fbm_rot * p * 2.0;
        amplitude *= 0.5;
    }
    return value;
}

float turbulence(vec3 p) {
    return turbulence(p, 5);
}

#endif // NOISE_GLSL
