#include "GraphicsOperator.hpp"

#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

namespace MayaFlux::Nodes::Network {

void GraphicsOperator::apply_one_to_one(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    size_t point_count = get_point_count();

    if (source->get_node_count() != point_count) {
        return;
    }

    if (param == "color") {
        for (size_t i = 0; i < point_count; ++i) {
            auto val = source->get_node_output(i);
            if (!val)
                continue;

            auto* point = static_cast<GpuSync::PointVertex*>(get_data_at(i));
            if (point) {
                float normalized = glm::clamp(static_cast<float>(*val), 0.0F, 1.0F);
                point->color = glm::vec3(normalized, 0.5F, 1.0F - normalized);
            }
        }
    } else if (param == "size") {
        for (size_t i = 0; i < point_count; ++i) {
            auto val = source->get_node_output(i);
            if (!val)
                continue;

            auto* point = static_cast<GpuSync::PointVertex*>(get_data_at(i));
            if (point) {
                point->size = glm::clamp(static_cast<float>(*val) * 10.0F, 1.0F, 50.0F);
            }
        }
    }
}
}
