#include "MeshNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Network/Operators/MeshOperator.hpp"

namespace MayaFlux::Nodes::Network {

// =============================================================================
// Construction
// =============================================================================

MeshNetwork::MeshNetwork()
{
    set_output_mode(OutputMode::GRAPHICS_BIND);
    set_topology(Topology::CUSTOM);
    m_operator_chain = std::make_shared<OperatorChain>();
}

// =============================================================================
// Slot management
// =============================================================================

uint32_t MeshNetwork::add_slot(
    std::string name,
    std::shared_ptr<GpuSync::MeshWriterNode> node,
    std::optional<uint32_t> parent)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "MeshNetwork::add_slot: null MeshWriterNode for slot '{}'", name);
    }

    if (parent.has_value() && parent.value() >= m_slots.size()) {
        error<std::out_of_range>(
            Journal::Component::Nodes, Journal::Context::Init,
            std::source_location::current(),
            "MeshNetwork::add_slot: parent index {} out of range (slot count: {})",
            parent.value(), m_slots.size());
    }

    const auto idx = static_cast<uint32_t>(m_slots.size());

    MeshSlot slot;
    slot.index = idx;
    slot.name = std::move(name);
    slot.node = std::move(node);
    slot.parent_index = parent;
    slot.dirty = true;

    if (parent.has_value()) {
        m_slots[parent.value()].child_indices.push_back(idx);
    }

    m_slots.push_back(std::move(slot));
    m_sort_dirty = true;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::Init,
        "MeshNetwork: added slot '{}' at index {} (parent: {})",
        m_slots[idx].name, idx,
        parent.has_value() ? std::to_string(parent.value()) : "none");

    return idx;
}

MeshSlot& MeshNetwork::get_slot(uint32_t index)
{
    return m_slots.at(index);
}

const MeshSlot& MeshNetwork::get_slot(uint32_t index) const
{
    return m_slots.at(index);
}

MeshSlot* MeshNetwork::find_slot(std::string_view name)
{
    auto it = std::ranges::find_if(m_slots,
        [name](const MeshSlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

const MeshSlot* MeshNetwork::find_slot(std::string_view name) const
{
    auto it = std::ranges::find_if(m_slots,
        [name](const MeshSlot& s) { return s.name == name; });
    return it != m_slots.end() ? &*it : nullptr;
}

std::optional<uint32_t> MeshNetwork::find_slot_index(std::string_view name) const
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_slots.size()); ++i) {
        if (m_slots[i].name == name)
            return i;
    }
    return std::nullopt;
}

// =============================================================================
// NodeNetwork interface
// =============================================================================

void MeshNetwork::process_batch(unsigned int num_samples)
{
    if (!is_enabled() || m_slots.empty())
        return;

    if (m_sort_dirty)
        rebuild_sort();

    for (unsigned int frame = 0; frame < num_samples; ++frame) {
        if (m_operator) {
            if (auto* mesh_op = dynamic_cast<MeshOperator*>(m_operator.get()))
                mesh_op->set_slots(m_slots, m_sorted_indices);
            m_operator->process(static_cast<float>(frame));
        } else {
            propagate_world_transforms();
        }

        if (m_operator_chain) {
            for (const auto& op : m_operator_chain->operators()) {
                if (auto* mesh_op = dynamic_cast<MeshOperator*>(op.get()))
                    mesh_op->set_slots(m_slots, m_sorted_indices);
                op->process(static_cast<float>(frame));
            }
        }
    }

    for (const auto& slot : m_slots) {
        if (slot.node)
            slot.node->compute_frame();
    }
}

void MeshNetwork::reset()
{
    for (auto& slot : m_slots) {
        slot.local_transform = glm::mat4(1.0F);
        slot.world_transform = glm::mat4(1.0F);
        slot.dirty = true;
    }
}

std::optional<double> MeshNetwork::get_node_output(size_t index) const
{
    if (index >= m_slots.size())
        return std::nullopt;
    return static_cast<double>(index);
}

std::unordered_map<std::string, std::string> MeshNetwork::get_metadata() const
{
    auto meta = NodeNetwork::get_metadata();
    meta["slot_count"] = std::to_string(m_slots.size());
    meta["operator"] = m_operator ? std::string(m_operator->get_type_name()) : "none";
    meta["chain_size"] = m_operator_chain
        ? std::to_string(m_operator_chain->size())
        : "0";
    return meta;
}

// =============================================================================
// Operator management
// =============================================================================

void MeshNetwork::set_operator(std::shared_ptr<NetworkOperator> op)
{
    if (!op) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Init,
            "MeshNetwork::set_operator: null operator ignored");
        return;
    }
    m_operator = std::move(op);
}

void MeshNetwork::ensure_sorted()
{
    if (m_sort_dirty)
        rebuild_sort();
}

// =============================================================================
// Private helpers
// =============================================================================

void MeshNetwork::rebuild_sort()
{
    // Kahn's algorithm: process roots first, then children.
    std::vector<uint32_t> in_degree(m_slots.size(), 0);
    for (const auto& slot : m_slots) {
        if (slot.parent_index.has_value())
            ++in_degree[slot.parent_index.value()];
    }

    // Roots: slots with no parent.
    std::vector<uint32_t> queue;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_slots.size()); ++i) {
        if (!m_slots[i].parent_index.has_value())
            queue.push_back(i);
    }

    m_sorted_indices.clear();
    m_sorted_indices.reserve(m_slots.size());

    while (!queue.empty()) {
        uint32_t idx = queue.front();
        queue.erase(queue.begin());
        m_sorted_indices.push_back(idx);

        for (uint32_t child : m_slots[idx].child_indices)
            queue.push_back(child);
    }

    if (m_sorted_indices.size() != m_slots.size()) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Init,
            "MeshNetwork: slot DAG contains a cycle — sort is incomplete");
    }

    m_sort_dirty = false;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::Init,
        "MeshNetwork: rebuilt slot sort order ({} slots)", m_sorted_indices.size());
}

void MeshNetwork::propagate_world_transforms()
{
    for (uint32_t idx : m_sorted_indices) {
        auto& slot = m_slots[idx];
        if (slot.parent_index.has_value()) {
            slot.world_transform
                = m_slots[slot.parent_index.value()].world_transform
                * slot.local_transform;
        } else {
            slot.world_transform = slot.local_transform;
        }
    }
}

} // namespace MayaFlux::Nodes::Network
