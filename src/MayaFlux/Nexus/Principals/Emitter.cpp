#include "Emitter.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

void Emitter::set_vertices(const void* data, size_t byte_count)
{
    for (auto& s : m_render_sinks)
        s.writer->set_vertices(data, byte_count);
}

void Emitter::attach(const std::shared_ptr<Buffers::VKBuffer>& buf, const AttachConfig& config)
{
    if (!buf) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::Init,
            "Cannot attach null buffer");
        return;
    }

    detach();
    m_attachment = make_attachment(buf, config, {});
    follow_attachment();
}

void Emitter::detach()
{
    if (m_attachment) {
        release_attachment(*m_attachment);
        m_attachment.reset();
    }
}

void Emitter::recenter()
{
    if (m_attachment) {
        recenter_attachment(*m_attachment, m_position, {});
    }
}

void Emitter::follow_attachment()
{
    if (m_attachment) {
        Nexus::follow_attachment(*m_attachment, m_position, {});
    }
}

} // namespace MayaFlux::Nexus
