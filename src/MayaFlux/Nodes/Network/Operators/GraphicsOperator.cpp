#include "GraphicsOperator.hpp"

#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

namespace MayaFlux::Nodes::Network {

namespace {

    void write_point(PointVertex& point, GraphicsOperator::PointAttribute attribute, double value)
    {
        if (attribute == GraphicsOperator::PointAttribute::COLOR) {
            const float normalized = glm::clamp(static_cast<float>(value), 0.0F, 1.0F);
            point.color = glm::vec4(normalized, 0.5F, 1.0F - normalized, 1.0F);
        } else {
            point.size = glm::clamp(static_cast<float>(value) * 10.0F, 1.0F, 50.0F);
        }
    }

}

[[nodiscard]] std::vector<DrawRun> GraphicsOperator::topology_runs() const
{
    const auto topo = declared_topology().value_or(Portal::Graphics::PrimitiveTopology::POINT_LIST);
    return { DrawRun {
        .topology = topo,
        .vertex_offset = 0,
        .vertex_count = static_cast<uint32_t>(get_vertex_count()) } };
}

void GraphicsOperator::apply_one_to_one(
    std::string_view param,
    const std::shared_ptr<NodeNetwork>& source)
{
    size_t point_count = get_point_count();

    if (source->get_node_count() != point_count) {
        return;
    }

    PointAttribute attribute {};
    if (param == "color") {
        attribute = PointAttribute::COLOR;
    } else if (param == "size") {
        attribute = PointAttribute::SIZE;
    } else {
        return;
    }

    for (size_t i = 0; i < point_count; ++i) {
        const auto value = source->get_node_output(i);
        if (!value) {
            continue;
        }
        if (auto* point = static_cast<PointVertex*>(get_data_at(i))) {
            write_point(*point, attribute, *value);
        }
    }
}

void GraphicsOperator::apply_point_control(Control& control, PointAttribute attribute)
{
    if (!control.due()) {
        return;
    }
    control.settle();

    const size_t point_count = get_point_count();
    if (control.per_element() && control.element_count() != point_count) {
        return;
    }

    for (size_t i = 0; i < point_count; ++i) {
        const auto value = control.at(i);
        if (!value) {
            continue;
        }
        if (auto* point = static_cast<PointVertex*>(get_data_at(i))) {
            write_point(*point, attribute, *value);
        }
    }
}
}
