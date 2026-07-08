#ifndef VISION_GRADIENTS_GLSL
#define VISION_GRADIENTS_GLSL

#include "vision_common.glsl"

#define SAMPLE_NEIGHBORHOOD_3X3(img, coord, tl, tc, tr, ml, mr, bl, bc, br) \
    do { \
        (tl) = sample_clamped((img), (coord) + ivec2(-1, -1)).r; \
        (tc) = sample_clamped((img), (coord) + ivec2(0, -1)).r; \
        (tr) = sample_clamped((img), (coord) + ivec2(1, -1)).r; \
        (ml) = sample_clamped((img), (coord) + ivec2(-1, 0)).r; \
        (mr) = sample_clamped((img), (coord) + ivec2(1, 0)).r; \
        (bl) = sample_clamped((img), (coord) + ivec2(-1, 1)).r; \
        (bc) = sample_clamped((img), (coord) + ivec2(0, 1)).r; \
        (br) = sample_clamped((img), (coord) + ivec2(1, 1)).r; \
    } while (false)

#define SOBEL_GRADIENT(img, coord, out_gx, out_gy) \
    do { \
        float _tl, _tc, _tr, _ml, _mr, _bl, _bc, _br; \
        SAMPLE_NEIGHBORHOOD_3X3((img), (coord), _tl, _tc, _tr, _ml, _mr, _bl, _bc, _br); \
        (out_gx) = -_tl + _tr - 2.0 * _ml + 2.0 * _mr - _bl + _br; \
        (out_gy) = -_tl - 2.0 * _tc - _tr + _bl + 2.0 * _bc + _br; \
    } while (false)

#endif // VISION_GRADIENTS_GLSL
