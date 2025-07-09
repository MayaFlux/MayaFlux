#include "BufferProcessor.hpp"
#include "Buffer.hpp"

namespace MayaFlux::Buffers {

void BufferProcessor::process(std::shared_ptr<Buffer> buffer)
{
    if (!buffer->try_acquire_processing()) {
        std::cerr << "Warning: Buffer already being processed, skipping processor" << std::endl;
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
}
