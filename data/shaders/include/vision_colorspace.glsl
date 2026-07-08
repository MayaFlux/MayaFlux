#ifndef VISION_COLORSPACE_GLSL
#define VISION_COLORSPACE_GLSL

/**
 * @brief Convert linear RGB to HSV.
 *
 * @param rgb Input colour, components in [0, 1].
 * @return    HSV: h in [0, 1) representing [0, 360), s and v in [0, 1].
 */
vec3 rgb_to_hsv(vec3 rgb)
{
    const float cmax = max(rgb.r, max(rgb.g, rgb.b));
    const float cmin = min(rgb.r, min(rgb.g, rgb.b));
    const float delta = cmax - cmin;

    float h = 0.0;
    if (delta > 1e-6) {
        if (cmax == rgb.r)
            h = mod((rgb.g - rgb.b) / delta, 6.0);
        else if (cmax == rgb.g)
            h = (rgb.b - rgb.r) / delta + 2.0;
        else
            h = (rgb.r - rgb.g) / delta + 4.0;
        h /= 6.0;
    }

    const float s = (cmax > 1e-6) ? (delta / cmax) : 0.0;
    return vec3(h, s, cmax);
}

/**
 * @brief Convert HSV back to linear RGB.
 *
 * @param hsv Input HSV: h in [0, 1), s and v in [0, 1].
 * @return    RGB, components in [0, 1].
 */
vec3 hsv_to_rgb(vec3 hsv)
{
    const float h6 = hsv.x * 6.0;
    const float s = hsv.y;
    const float v = hsv.z;
    const float c = v * s;
    const float x = c * (1.0 - abs(mod(h6, 2.0) - 1.0));
    const float m = v - c;

    vec3 rgb;
    if (h6 < 1.0) rgb = vec3(c, x, 0.0);
    else if (h6 < 2.0) rgb = vec3(x, c, 0.0);
    else if (h6 < 3.0) rgb = vec3(0.0, c, x);
    else if (h6 < 4.0) rgb = vec3(0.0, x, c);
    else if (h6 < 5.0) rgb = vec3(x, 0.0, c);
    else rgb = vec3(c, 0.0, x);

    return rgb + m;
}

#endif // VISION_COLORSPACE_GLSL
