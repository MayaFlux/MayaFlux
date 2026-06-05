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

void MeshFieldOperator::set_gpu_executor(
    std::shared_ptr<Yantra::ShaderExecutionContext<>> executor, bool continuous)
{
    if (!executor) {
        m_compute_node.reset();
        m_executor.reset();
        m_gpu_slot_vertex_counts.clear();
        return;
    }

    m_gpu_slot_vertex_counts.clear();
    if (m_slots) {
        m_gpu_slot_vertex_counts.reserve(m_slots->size());
        for (const auto& slot : *m_slots) {
            m_gpu_slot_vertex_counts.push_back(
                slot.node ? slot.node->get_mesh_vertex_count() : 0);
        }
    }

    m_executor = std::move(executor);
    m_compute_node = std::make_shared<Nodes::GpuSync::GpuComputeNode>(m_executor, continuous);

    m_compute_node->on_complete([this](Nodes::GpuSync::GpuComputeContext& ctx) {
        if (!m_slots)
            return;

        const auto& primary = ctx.gpu_result.primary;
        constexpr size_t floats_per_vertex = sizeof(MeshVertex) / sizeof(float);

        size_t offset = 0;
        for (size_t i = 0; i < m_slots->size(); ++i) {
            if (i >= m_gpu_slot_vertex_counts.size())
                break;

            const size_t vc = m_gpu_slot_vertex_counts[i];
            const size_t float_count = vc * floats_per_vertex;

            if (offset + float_count > primary.size())
                break;

            auto& slot = (*m_slots)[i];
            if (!slot.node) {
                offset += float_count;
                continue;
            }

            std::vector<MeshVertex> verts(vc);
            std::memcpy(verts.data(), primary.data() + offset,
                float_count * sizeof(float));
            slot.node->set_mesh_vertices(std::move(verts));

            offset += float_count;
        }
    });

    m_compute_node->set_dirty();
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

void MeshFieldOperator::process(float dt)
{
    if (m_compute_node) {
        m_compute_node->compute_frame();
        return;
    }

    MeshOperator::process(dt);
}

} // namespace MayaFlux::Nodes::Network
