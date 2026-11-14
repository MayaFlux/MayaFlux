#pragma once

#include "MayaFlux/IO/GlmTextCodec.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"
#include "MayaFlux/Transitive/Reflect/RecordBinding.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct VertexGeometryInputs
 * @brief A topology and ordered vertices in the Kakshya::Vertex field format.
 */
struct VertexGeometryInputs {
    Graphics::PrimitiveTopology topology { Graphics::PrimitiveTopology::POINT_LIST };
    std::vector<Kakshya::Vertex> vertices;
};

MF_DESCRIBE(VertexGeometryInputs)

/**
 * @struct RectPlacementInputs
 * @brief A placement by origin and size: x, y, width, height.
 */
struct RectPlacementInputs {
    glm::vec4 rect {};
};

MF_DESCRIBE(RectPlacementInputs)

/**
 * @struct PolygonHole
 * @brief One inner ring cut out of an outline, with optional per point colors.
 */
struct PolygonHole {
    std::vector<glm::vec2> points;
    std::vector<glm::vec4> colors;
};

MF_DESCRIBE(PolygonHole)

/**
 * @struct PolygonInputs
 * @brief An outline in unit space, filled or stroked, with a bool state.
 *
 * points and the holes' points are x y pairs in the unit square, which the
 * placement's rect maps onto the surface. colors is empty or one per point,
 * and a hole follows its outline. A thickness above zero draws the rings as a
 * line loop instead of filling them. While the state is true the active color
 * replaces every vertex color. containment and interaction are names a table resolves.
 */
struct PolygonInputs {
    std::vector<glm::vec2> points;
    std::vector<glm::vec4> colors;
    glm::vec4 color { 1.F };
    std::optional<glm::vec4> active;
    float thickness { 0.F };
    bool on { false };
    std::string containment;
    std::string interaction;
    std::vector<PolygonHole> holes;
};

MF_DESCRIBE(PolygonInputs)

}

#include "StaticGeometryInputs.describe.hpp"

namespace MayaFlux::Portal::Forma::Detail {

inline bool vertex_count_matches_topology(Graphics::PrimitiveTopology topology, size_t count)
{
    using Graphics::PrimitiveTopology;
    switch (topology) {
    case PrimitiveTopology::POINT_LIST:
        return count >= 1;
    case PrimitiveTopology::LINE_LIST:
        return count >= 2 && count % 2 == 0;
    case PrimitiveTopology::LINE_STRIP:
        return count >= 2;
    case PrimitiveTopology::TRIANGLE_LIST:
        return count >= 3 && count % 3 == 0;
    case PrimitiveTopology::TRIANGLE_STRIP:
    case PrimitiveTopology::TRIANGLE_FAN:
        return count >= 3;
    }
    return false;
}

}
