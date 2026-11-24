#include "NetworkGeometryBuffer.hpp"

#include "SpatialFieldWiring.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Network/ParticleNetwork.hpp"
#include "MayaFlux/Nodes/Network/PointCloudNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/OperatorChain.hpp"
#include "MayaFlux/Nodes/Network/Operators/PhysicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {

    /**
     * @brief Whether this network type participates in GpuFieldOperator
     *        chain wiring at all.
     *
     * ParticleNetwork and PointCloudNetwork both aggregate an unordered
     * point set the field dispatch and spatial-hash stages address
     * uniformly. MeshNetwork and InstanceNetwork carry structured
     * connectivity none of these stages model, so their buffers skip the
     * scan. Subclass if this list ever needs to diverge per network.
     */
    bool is_field_compatible_network(const Nodes::Network::NodeNetwork* network)
    {
        return dynamic_cast<const Nodes::Network::ParticleNetwork*>(network) != nullptr
            || dynamic_cast<const Nodes::Network::PointCloudNetwork*>(network) != nullptr;
    }

} // namespace

void NetworkGeometryBuffer::wire_field_operators()
{
    if (!is_field_compatible_network(m_network.get())) {
        return;
    }

    auto op_chain = m_network ? m_network->get_operator_chain() : nullptr;
    if (!op_chain) {
        return;
    }

    std::shared_ptr<Nodes::Network::GpuFieldOperator> field_op;
    for (const auto& op : op_chain->operators()) {
        field_op = std::dynamic_pointer_cast<Nodes::Network::GpuFieldOperator>(op);
        if (field_op) {
            break;
        }
    }
    if (!field_op) {
        return;
    }

    auto self = std::dynamic_pointer_cast<NetworkGeometryBuffer>(shared_from_this());
    if (!self || !get_processing_chain()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
            "NetworkGeometryBuffer: no processing chain to wire field stages onto");
        return;
    }

    auto* primary = get_network()->get_operator();
    auto* physics = dynamic_cast<Nodes::Network::PhysicsOperator*>(primary);
    auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(primary);

    const auto& pconfig = field_op->get_field_config();

    std::optional<SpatialHashConfig> hash_config;
    if (wants_spatial_stages(pconfig)) {
        std::optional<float> cell_size = pconfig.cell_size;
        if (!cell_size.has_value() && physics) {
            cell_size = physics->get_interaction_radius();
        }

        if (!cell_size.has_value()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                "NetworkGeometryBuffer: SpatialFieldConfig has no cell_size and the network's "
                "primary operator is not a PhysicsOperator, so one cannot be derived");
        } else {
            hash_config = SpatialHashConfig::from_network(self, *cell_size);
            if (!hash_config.has_value()) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::Init,
                    "NetworkGeometryBuffer: SpatialHashConfig::from_network failed");
            }
        }
    }

    std::vector<uint32_t> cluster_ids;
    if (hash_config.has_value() && graphics_op) {
        cluster_ids = graphics_op->build_cluster_ids();
    }

    wire_field_stages(self, field_op, hash_config, cluster_ids, physics);
}

} // namespace MayaFlux::Buffers
