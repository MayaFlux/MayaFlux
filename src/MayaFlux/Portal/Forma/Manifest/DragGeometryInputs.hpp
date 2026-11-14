#pragma once

#include "PlacementInputs.hpp"

#include "MayaFlux/IO/GlmTextCodec.hpp"
#include "MayaFlux/Transitive/Reflect/RecordBinding.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct ScaleInputs
 * @brief An outline in unit space that grows along one axis with a float state.
 *
 * points are x y pairs of a single ring. The state, clamped to [0, 1], scales
 * every point along x when horizontal and along y otherwise, and the
 * placement's rect maps the result onto the surface. value is the initial
 * state.
 */
struct ScaleInputs {
    std::vector<glm::vec2> points;
    glm::vec4 color { 1.F };
    bool horizontal { true };
    float value { 0.F };
};

MF_DESCRIBE(ScaleInputs)

/**
 * @struct HandlePathInputs
 * @brief Draggable points in the unit square, drawn as a connected stroke.
 *
 * points are the initial positions and also the state. grab_radius and
 * handle_size are in NDC units.
 */
struct HandlePathInputs {
    std::vector<glm::vec2> points;
    glm::vec3 path_color { 0.3F, 0.7F, 1.F };
    glm::vec3 handle_color { 0.95F };
    float thickness { 1.5F };
    float handle_size { 0.03F };
    float grab_radius { 0.1F };
};

MF_DESCRIBE(HandlePathInputs)

}

#include "DragGeometryInputs.describe.hpp"
