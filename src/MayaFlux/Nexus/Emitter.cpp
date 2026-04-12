#include "Emitter.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

void Emitter::set_influence_target(std::shared_ptr<Buffers::RenderProcessor> proc)
{
    if (!proc) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Cannot set null influence target");
        return;
    }

    if (m_influence_ubo) {
        clear_influence_target();
    }

    m_influence_ubo = std::make_shared<Buffers::VKBuffer>(
        sizeof(InfluenceUBO),
        Buffers::VKBuffer::Usage::UNIFORM,
        Kakshya::DataModality::UNKNOWN);

    proc->add_binding("u_influence",
        Buffers::ShaderBinding { 1, 1, vk::DescriptorType::eUniformBuffer });

    proc->bind_buffer("u_influence", m_influence_ubo);

    m_influence_target = std::move(proc);
}

void Emitter::clear_influence_target()
{
    m_influence_target->unbind_buffer("u_influence");
    m_influence_ubo.reset();
    m_influence_target.reset();
}

void Emitter::upload_influence_ubo(const InfluenceContext& ctx) const
{
    if (!m_influence_ubo || !m_influence_ubo->get_mapped_ptr()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "Cannot upload influence UBO: no target or failed to map buffer");
        return;
    }

    InfluenceUBO data {
        .position = ctx.position,
        .intensity = ctx.intensity,
        .color = ctx.color.value_or(glm::vec3(1.0F)),
        .radius = ctx.radius,
        .size = ctx.size.value_or(1.0F),
    };

    Buffers::upload_to_gpu(&data, sizeof(InfluenceUBO), m_influence_ubo, nullptr);
}

} // namespace MayaFlux::Nexus
