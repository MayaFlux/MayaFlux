#include "MeshFieldOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Network/MeshSlot.hpp"

namespace MayaFlux::Nodes::Network {

// =============================================================================
// Private helpers
// =============================================================================

std::shared_ptr<FieldOperator>
MeshFieldOperator::get_or_create(MeshSlot& slot, uint32_t slot_index)
{
    auto it = m_field_ops.find(slot_index);
    if (it != m_field_ops.end())
        return it->second;

    auto op = std::make_shared<FieldOperator>(FieldMode::ABSOLUTE);

    if (slot.node) {
        op->initialize(slot.node->get_mesh_vertices());
        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "MeshFieldOperator: initialised FieldOperator for slot {} ({} vertices)",
            slot_index, slot.node->get_mesh_vertex_count());
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "MeshFieldOperator: slot {} has no node at bind time -- "
            "FieldOperator initialised with empty vertex data",
            slot_index);
    }

    m_field_ops.emplace(slot_index, op);
    return op;
}

// =============================================================================
// Field binding
// =============================================================================

void MeshFieldOperator::bind(
    uint32_t slot_index, FieldTarget target, Kinesis::VectorField field)
{
    if (!m_slots) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "MeshFieldOperator::bind called before set_slots() -- "
            "FieldOperator will be initialised lazily on first process()");
    }

    if (m_slots) {
        if (slot_index >= m_slots->size()) {
            MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                "MeshFieldOperator::bind: slot_index {} out of range ({})",
                slot_index, m_slots->size());
            return;
        }
        auto op = get_or_create((*m_slots)[slot_index], slot_index);
        op->bind(target, std::move(field));
    } else {
        auto& op = m_field_ops[slot_index];
        if (!op)
            op = std::make_shared<FieldOperator>(FieldMode::ABSOLUTE);
        op->bind(target, std::move(field));
    }
}

void MeshFieldOperator::bind(
    uint32_t slot_index, FieldTarget target, Kinesis::SpatialField field)
{
    if (m_slots && slot_index < m_slots->size()) {
        get_or_create((*m_slots)[slot_index], slot_index)->bind(target, std::move(field));
    } else {
        auto& op = m_field_ops[slot_index];
        if (!op)
            op = std::make_shared<FieldOperator>(FieldMode::ABSOLUTE);
        op->bind(target, std::move(field));
    }
}

void MeshFieldOperator::bind(
    uint32_t slot_index, FieldTarget target, Kinesis::UVField field)
{
    if (m_slots && slot_index < m_slots->size()) {
        get_or_create((*m_slots)[slot_index], slot_index)->bind(target, std::move(field));
    } else {
        auto& op = m_field_ops[slot_index];
        if (!op)
            op = std::make_shared<FieldOperator>(FieldMode::ABSOLUTE);
        op->bind(target, std::move(field));
    }
}

void MeshFieldOperator::unbind(uint32_t slot_index, FieldTarget target)
{
    auto it = m_field_ops.find(slot_index);
    if (it != m_field_ops.end())
        it->second->unbind(target);
}

void MeshFieldOperator::unbind_slot(uint32_t slot_index)
{
    m_field_ops.erase(slot_index);
}

void MeshFieldOperator::unbind_all()
{
    m_field_ops.clear();
}

void MeshFieldOperator::set_mode(uint32_t slot_index, FieldMode mode)
{
    auto it = m_field_ops.find(slot_index);
    if (it != m_field_ops.end())
        it->second->set_mode(mode);
}

// =============================================================================
// MeshOperator interface
// =============================================================================

void MeshFieldOperator::process_slot(MeshSlot& slot, float dt)
{
    auto it = m_field_ops.find(slot.index);
    if (it == m_field_ops.end())
        return;

    auto& field_op = it->second;

    if (field_op->get_vertex_count() == 0 && slot.node
        && slot.node->get_mesh_vertex_count() > 0) {
        field_op->initialize(slot.node->get_mesh_vertices());
        MF_DEBUG(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "MeshFieldOperator: lazy-initialised slot {} ({} vertices)",
            slot.index, slot.node->get_mesh_vertex_count());
    }

    field_op->process(dt);

    if (field_op->is_vertex_data_dirty() && slot.node) {
        slot.node->set_mesh_vertices(field_op->extract_mesh_vertices());
        field_op->mark_vertex_data_clean();
    }
}

} // namespace MayaFlux::Nodes::Network
