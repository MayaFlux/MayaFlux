#include "AssemblyNetwork.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

AssemblyNetwork::AssemblyNetwork()
    : m_operator(std::make_unique<AssemblyOperator>())
{
    set_topology(Topology::INDEPENDENT);
    set_output_mode(OutputMode::GRAPHICS_BIND);
    m_operator_chain = std::make_shared<OperatorChain>();

    MF_INFO(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "Created empty AssemblyNetwork");
}

void AssemblyNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || !m_operator) {
        return;
    }

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        m_operator->process(0.0F);
    }

    if (m_operator_chain && !m_operator_chain->empty()) {
        for (unsigned int frame = 0; frame < num_samples; ++frame) {
            m_operator_chain->process(0.0F, m_operator.get());
        }
    }
}

size_t AssemblyNetwork::get_node_count() const
{
    return m_operator ? m_operator->item_count() : 0;
}

} // namespace MayaFlux::Nodes::Network
