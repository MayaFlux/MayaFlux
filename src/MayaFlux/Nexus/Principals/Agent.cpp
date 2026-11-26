#include "Agent.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

PlacementControls& Agent::open_placement(QuantityStore& store)
{
    return store.declare<PlacementControls>();
}

AgentControls& Agent::open(QuantityStore& store)
{
    auto& controls = store.extend<AgentControls>();
    controls.intensity.observe(1.0F);
    controls.radius.observe(1.0F);
    return controls;
}

void Agent::set_vertices(const void* data, size_t byte_count)
{
    for (auto& s : m_render_sinks)
        s.writer->set_vertices(data, byte_count);
}

void Agent::attach(const std::shared_ptr<Buffers::VKBuffer>& buf, const AttachConfig& config)
{
    if (!buf) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Cannot attach null buffer");
        return;
    }

    detach();
    m_attachment = make_attachment(buf, config, orientation());
    follow_attachment();
}

void Agent::detach()
{
    if (m_attachment) {
        release_attachment(*m_attachment);
        m_attachment.reset();
    }
}

void Agent::recenter()
{
    if (!m_attachment) {
        return;
    }
    auto placed = position();
    recenter_attachment(*m_attachment, placed, orientation());
    if (placed) {
        m_placement.position.observe(*placed);
    }
}

void Agent::follow_attachment()
{
    if (!m_attachment) {
        return;
    }
    auto placed = position();
    Nexus::follow_attachment(*m_attachment, placed, orientation());
    if (placed) {
        m_placement.position.observe(*placed);
    }
}

} // namespace MayaFlux::Nexus
