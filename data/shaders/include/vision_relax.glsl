#ifndef VISION_RELAX_GLSL
#define VISION_RELAX_GLSL

/**
 * @brief Iterate the 8-neighborhood of coord, bounds-checked against w/h.
 *        Executes body once per valid neighbor with nc bound to its
 *        coordinate. Defined as a macro since GLSL has no closures.
 */
#define FOR_EACH_8NEIGHBOR(coord, w, h, body) \
    do { \
        for (int _dy = -1; _dy <= 1; ++_dy) { \
            for (int _dx = -1; _dx <= 1; ++_dx) { \
                if (_dx == 0 && _dy == 0) continue; \
                ivec2 nc = (coord) + ivec2(_dx, _dy); \
                if (nc.x < 0 || nc.y < 0 || nc.x >= int(w) || nc.y >= int(h)) continue; \
                body \
            } \
        } \
    } while (false)

/**
 * @brief Report this invocation's change flag to changed_buf via one
 *        subgroup-reduced atomic write per subgroup rather than per thread.
 *        Requires GL_KHR_shader_subgroup_basic and
 *        GL_KHR_shader_subgroup_arithmetic.
 */
#define REPORT_CHANGED(changed_here, changed_buf) \
    do { \
        bool _sg_changed = subgroupOr(changed_here); \
        if (_sg_changed && subgroupElect()) \
            atomicMax((changed_buf)[0], 1u); \
    } while (false)

#endif // VISION_RELAX_GLSL
