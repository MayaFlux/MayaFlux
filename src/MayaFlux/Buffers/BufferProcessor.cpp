#include "BufferProcessor.hpp"
#include "Buffer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void BufferProcessor::process(const std::shared_ptr<Buffer>& buffer)
{
    if (!buffer->try_acquire_processing()) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Buffer is already being processed, skipping processor");
        return;
    }

    m_active_processing.fetch_add(1, std::memory_order_acquire);

    try {
        processing_function(buffer);
    } catch (...) {
        buffer->release_processing();
        m_active_processing.fetch_sub(1, std::memory_order_release);
        throw;
    }

    buffer->release_processing();
    m_active_processing.fetch_sub(1, std::memory_order_release);
}

void BufferProcessor::process_non_owning(const std::shared_ptr<Buffer>& buffer)
{
    m_active_processing.fetch_add(1, std::memory_order_acquire);

    try {
        processing_function(buffer);
    } catch (...) {
        m_active_processing.fetch_sub(1, std::memory_order_release);
        throw;
    }

    m_active_processing.fetch_sub(1, std::memory_order_release);
}
}
