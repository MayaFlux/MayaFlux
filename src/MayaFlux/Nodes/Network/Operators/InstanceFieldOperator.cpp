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

void InstanceFieldOperator::set_gpu_executor(
    std::shared_ptr<Yantra::ShaderExecutionContext<>> executor, bool continuous)
{
    if (!executor) {
        m_compute_node.reset();
        m_executor.reset();
        return;
    }

    m_executor = std::move(executor);
    m_compute_node = std::make_shared<Nodes::GpuSync::GpuComputeNode>(m_executor, continuous);

    m_compute_node->on_complete([this](Nodes::GpuSync::GpuComputeContext& ctx) {
        if (!m_slots)
            return;

        const auto& primary = ctx.gpu_result.primary;
        const size_t floats_per_slot = 16;

        for (size_t i = 0; i < m_slots->size(); ++i) {
            const size_t base = i * floats_per_slot;
            if (base + floats_per_slot > primary.size())
                break;

            auto& slot = (*m_slots)[i];
            std::memcpy(glm::value_ptr(slot.transform), primary.data() + base,
                floats_per_slot * sizeof(float));
            slot.dirty = true;
        }
    });

    m_compute_node->set_dirty();
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

void InstanceFieldOperator::process(float dt)
{
    if (m_compute_node) {
        m_compute_node->compute_frame();
        return;
    }

    InstanceOperator::process(dt);
}

} // namespace MayaFlux::Nodes::Network
