#include "MeshTransformOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

// =============================================================================
// Field binding
// =============================================================================

void MeshTransformOperator::bind(uint32_t slot_index, TransformField field)
{
    m_fields[slot_index] = std::move(field);
    if (!m_accumulated_time.contains(slot_index))
        m_accumulated_time[slot_index] = 0.0F;

    MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
        "MeshTransformOperator: bound field to slot {}", slot_index);
}

void MeshTransformOperator::unbind(uint32_t slot_index)
{
    m_fields.erase(slot_index);
    m_accumulated_time.erase(slot_index);
}

void MeshTransformOperator::unbind_all()
{
    m_fields.clear();
    m_accumulated_time.clear();
}

// =============================================================================
// MeshOperator interface
// =============================================================================

void MeshTransformOperator::process_slot(MeshSlot& slot, float dt)
{
    auto field_it = m_fields.find(slot.index);
    if (field_it != m_fields.end() && field_it->second) {
        float& t = m_accumulated_time[slot.index];
        t += dt;
        slot.local_transform = field_it->second(t);
    }

    slot.world_transform = parent_world(slot) * slot.local_transform;
    slot.dirty = true;
}

// =============================================================================
// Private helpers
// =============================================================================

glm::mat4 MeshTransformOperator::parent_world(const MeshSlot& slot) const
{
    if (!slot.parent_index.has_value() || !m_slots)
        return glm::mat4(1.0F);

    const uint32_t pidx = *slot.parent_index;
    if (pidx >= m_slots->size())
        return glm::mat4(1.0F);

    return (*m_slots)[pidx].world_transform;
}

} // namespace MayaFlux::Nodes::Network
