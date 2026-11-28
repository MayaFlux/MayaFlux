#ifndef MF_RADIOMETRY_TONE_GLSL
#define MF_RADIOMETRY_TONE_GLSL

/** Mirrors Kinesis/Radiometry/ToneCurve.hpp and the sRGB pair of ColorSpace.hpp. */

/** Scale linear colour by a number of stops. */
vec3 scale_by_stops(vec3 color, float stops)
{
    return color * exp2(stops);
}

/** Reinhard's curve with a white point, per channel. */
vec3 reinhard_extended(vec3 color, float white)
{
    return color * (vec3(1.0) + color / (white * white)) / (vec3(1.0) + color);
}

/** Khronos PBR Neutral, linear Rec. 709 in and out. */
vec3 pbr_neutral(vec3 color)
{
    const float start = 0.8 - 0.04;
    const float desaturation = 0.15;

    float low = min(color.r, min(color.g, color.b));
    color -= low < 0.08 ? low - 6.25 * low * low : 0.04;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < start) {
        return color;
    }

    const float d = 1.0 - start;
    float new_peak = 1.0 - d * d / (peak + d - start);
    color *= new_peak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - new_peak) + 1.0);
    return mix(color, vec3(new_peak), g);
}

/** Decode sRGB encoded components to linear. */
vec3 srgb_to_linear(vec3 encoded)
{
    vec3 low = encoded / 12.92;
    vec3 high = pow(max((encoded + 0.055) / 1.055, vec3(0.0)), vec3(2.4));
    return mix(high, low, lessThanEqual(encoded, vec3(0.04045)));
}

/** Encode linear components as sRGB. */
vec3 linear_to_srgb(vec3 linear)
{
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(max(linear, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(high, low, lessThanEqual(linear, vec3(0.0031308)));
}

#endif
