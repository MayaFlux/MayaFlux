#include "InstanceNetwork.hpp"

#include "Operators/InstanceOperator.hpp"

#include "MayaFlux/Transitive/Reflect/TypeInfo.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

InstanceNetwork::InstanceNetwork()
{
    set_output_mode(OutputMode::GRAPHICS_BIND);
    m_operator_chain = std::make_shared<OperatorChain>();
}

uint32_t InstanceNetwork::add_slot(
    std::string name, std::shared_ptr<GpuSync::GeometryWriterNode> node)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "InstanceNetwork::add_slot: null GeometryWriterNode for slot '{}'", name);
    }

    const auto idx = static_cast<uint32_t>(m_slots.size());
    if (name.empty()) {
        name = "slot_" + std::to_string(idx) + "_" + Reflect::short_dynamic_type_name(*node);
    }
    m_slots.push_back({ .index = idx, .name = std::move(name), .node = std::move(node) });
    return idx;
}

GeometrySlot& InstanceNetwork::get_slot(uint32_t index) { return m_slots.at(index); }
const GeometrySlot& InstanceNetwork::get_slot(uint32_t index) const { return m_slots.at(index); }

GeometrySlot* InstanceNetwork::find_slot(std::string_view name)
{
    auto it = std::ranges::find_if(
        m_slots, [name](const GeometrySlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

const GeometrySlot* InstanceNetwork::find_slot(std::string_view name) const
{
    auto it = std::ranges::find_if(
        m_slots, [name](const GeometrySlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

void InstanceNetwork::set_operator(std::shared_ptr<NetworkOperator> op)
{
    m_operator = std::move(op);
}

void InstanceNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || m_slots.empty())
        return;

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        if (m_operator) {
            if (auto* inst_op = dynamic_cast<InstanceOperator*>(m_operator.get()))
                inst_op->set_slots(m_slots);
            m_operator->process(0.0F);
        }

        if (m_operator_chain) {
            for (const auto& op : m_operator_chain->operators()) {
                if (auto* inst_op = dynamic_cast<InstanceOperator*>(op.get()))
                    inst_op->set_slots(m_slots);
                op->process(0.0F);
            }
        }
    }

    for (const auto& slot : m_slots) {
        if (slot.node)
            slot.node->compute_frame();
    }
}

void InstanceNetwork::reset()
{
    for (auto& slot : m_slots) {
        slot.transform = glm::mat4(1.0F);
        slot.dirty = true;
    }
}

std::optional<double> InstanceNetwork::get_node_output(size_t index) const
{
    if (index >= m_slots.size())
        return std::nullopt;
    return static_cast<double>(index);
}

std::unordered_map<std::string, std::string> InstanceNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();
    meta["slot_count"] = std::to_string(m_slots.size());
    meta["operator"] = m_operator ? std::string(m_operator->get_type_name()) : "none";
    meta["chain_size"] = m_operator_chain
        ? std::to_string(m_operator_chain->size())
        : "0";
    return meta;
}

} // namespace MayaFlux::Nodes::Network
