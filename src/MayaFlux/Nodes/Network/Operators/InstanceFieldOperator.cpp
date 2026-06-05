#include "InstanceFieldOperator.hpp"

namespace MayaFlux::Nodes::Network {

void InstanceFieldOperator::bind_transform(uint32_t slot_index, TransformField field)
{
    m_position_fields.erase(slot_index);
    m_transform_fields[slot_index] = std::move(field);
}

void InstanceFieldOperator::bind_position(uint32_t slot_index, PositionField field)
{
    m_transform_fields.erase(slot_index);
    m_position_fields[slot_index] = std::move(field);
}

void InstanceFieldOperator::unbind(uint32_t slot_index)
{
    m_transform_fields.erase(slot_index);
    m_position_fields.erase(slot_index);
}

void InstanceFieldOperator::unbind_all()
{
    m_transform_fields.clear();
    m_position_fields.clear();
}

void InstanceFieldOperator::process_slot(GeometrySlot& slot, float /*dt*/)
{
    if (auto it = m_transform_fields.find(slot.index);
        it != m_transform_fields.end() && it->second) {
        slot.transform = it->second(slot.transform);
        slot.dirty = true;
        return;
    }

    if (auto it = m_position_fields.find(slot.index);
        it != m_position_fields.end() && it->second.fn) {
        const glm::vec3 pos { slot.transform[3] };
        slot.transform[3] = glm::vec4(it->second(pos), 1.0F);
        slot.dirty = true;
    }
}

} // namespace MayaFlux::Nodes::Network
