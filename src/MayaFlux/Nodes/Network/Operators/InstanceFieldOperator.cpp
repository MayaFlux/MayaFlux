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

void InstanceFieldOperator::bind_tint(uint32_t slot_index, TintField field)
{
    m_tint_fields[slot_index] = std::move(field);
}

void InstanceFieldOperator::bind_scalar(uint32_t slot_index, ScalarField field)
{
    m_scalar_fields[slot_index] = std::move(field);
}

void InstanceFieldOperator::bind_texture(uint32_t slot_index, TextureField field)
{
    m_texture_fields[slot_index] = std::move(field);
}

void InstanceFieldOperator::unbind(uint32_t slot_index)
{
    m_transform_fields.erase(slot_index);
    m_position_fields.erase(slot_index);
    m_tint_fields.erase(slot_index);
    m_scalar_fields.erase(slot_index);
    m_texture_fields.erase(slot_index);
}

void InstanceFieldOperator::unbind_all()
{
    m_transform_fields.clear();
    m_position_fields.clear();
    m_tint_fields.clear();
    m_scalar_fields.clear();
    m_texture_fields.clear();
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
    } else if (auto position = m_position_fields.find(slot.index);
        position != m_position_fields.end() && position->second.fn) {
        const glm::vec3 pos { slot.transform[3] };
        slot.transform[3] = glm::vec4(position->second(pos), 1.0F);
        slot.dirty = true;
    }

    if (auto it = m_tint_fields.find(slot.index); it != m_tint_fields.end() && it->second) {
        const glm::vec3 tint = it->second(slot.tint);
        if (tint != slot.tint) {
            slot.tint = tint;
            slot.dirty = true;
        }
    }

    if (auto it = m_scalar_fields.find(slot.index); it != m_scalar_fields.end() && it->second) {
        const float scalar = it->second(slot.scalar);
        if (scalar != slot.scalar) {
            slot.scalar = scalar;
            slot.dirty = true;
        }
    }

    if (auto it = m_texture_fields.find(slot.index); it != m_texture_fields.end() && it->second) {
        const uint32_t texture = it->second(slot.texture);
        if (texture != slot.texture) {
            slot.texture = texture;
            slot.dirty = true;
        }
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
