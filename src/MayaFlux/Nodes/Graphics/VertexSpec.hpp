#pragma once

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Nodes {

struct PointVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float size = 10.0F;
};

struct LineVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float thickness = 2.0F;
    glm::vec2 uv = glm::vec2(0.F);
};

} // MayaFlux::Nodes
