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

} // namespace MayaFlux::Nexus
