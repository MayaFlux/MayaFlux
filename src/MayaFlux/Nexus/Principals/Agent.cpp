#include "Agent.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

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
    m_attachment = make_attachment(buf, config, m_orientation);
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
    if (m_attachment) {
        recenter_attachment(*m_attachment, m_position, m_orientation);
    }
}

void Agent::follow_attachment()
{
    if (m_attachment) {
        Nexus::follow_attachment(*m_attachment, m_position, m_orientation);
    }
}

} // namespace MayaFlux::Nexus
