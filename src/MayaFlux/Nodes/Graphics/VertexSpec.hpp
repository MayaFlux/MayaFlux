#pragma once

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Nodes::GpuSync {

struct PointVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float size = 10.0F;
};

struct LineVertex {
    glm::vec3 position;
    glm::vec3 color = glm::vec3(1.0F);
    float thickness = 2.0F;
};

} // MayaFlux::Nodes::GpuSync
