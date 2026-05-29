#include "Agent.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

void Agent::add_influence_target(std::shared_ptr<Buffers::RenderProcessor> proc)
{
    if (!proc) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Cannot add null influence target");
        return;
    }

    if (std::ranges::find(m_influence_targets, proc) != m_influence_targets.end()) {
        return;
    }

    if (!m_influence_ubo) {
        m_influence_ubo = std::make_shared<Buffers::VKBuffer>(
            sizeof(InfluenceUBO),
            Buffers::VKBuffer::Usage::UNIFORM,
            Kakshya::DataModality::UNKNOWN);
    }

    proc->add_binding("u_influence",
        Buffers::ShaderBinding { 1, 0, vk::DescriptorType::eUniformBuffer });

    proc->bind_buffer("u_influence", m_influence_ubo);

    m_influence_targets.push_back(std::move(proc));
}

void Agent::remove_influence_target(const std::shared_ptr<Buffers::RenderProcessor>& proc)
{
    auto it = std::ranges::find(m_influence_targets, proc);
    if (it == m_influence_targets.end())
        return;

    (*it)->unbind_buffer("u_influence");
    m_influence_targets.erase(it);

    if (m_influence_targets.empty())
        m_influence_ubo.reset();
}

void Agent::clear_influence_targets()
{
    for (const auto& proc : m_influence_targets)
        proc->unbind_buffer("u_influence");

    m_influence_targets.clear();
    m_influence_ubo.reset();
}

void Agent::upload_influence_ubo(const InfluenceContext& ctx) const
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

    std::memcpy(m_influence_ubo->get_mapped_ptr(), &data, sizeof(InfluenceUBO));
}

} // namespace MayaFlux::Nexus
