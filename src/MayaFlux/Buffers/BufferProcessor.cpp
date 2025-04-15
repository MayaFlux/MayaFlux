#include "BufferProcessor.hpp"
#include "AudioBuffer.hpp"

namespace MayaFlux::Buffers {

void BufferProcessingChain::add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    processor->on_attach(buffer);
    m_buffer_processors[buffer].push_back(processor);
}

void BufferProcessingChain::remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    auto it = m_buffer_processors.find(buffer);
    if (it != m_buffer_processors.end()) {
        auto& processors = it->second;
        auto proc_it = std::find(processors.begin(), processors.end(), processor);

        if (proc_it != processors.end()) {
            processor->on_detach(buffer);
            processors.erase(proc_it);

            if (processors.empty()) {
                m_buffer_processors.erase(it);
            }
        }
    }
}

void BufferProcessingChain::process(std::shared_ptr<AudioBuffer> buffer)
{
    auto it = m_buffer_processors.find(buffer);
    if (it != m_buffer_processors.end()) {
        for (auto& processor : it->second) {
            processor->process(buffer);
        }
    }
}

bool BufferProcessingChain::has_processors(std::shared_ptr<AudioBuffer> buffer) const
{
    auto it = m_buffer_processors.find(buffer);
    return it != m_buffer_processors.end() && !it->second.empty();
}

const std::vector<std::shared_ptr<BufferProcessor>>& BufferProcessingChain::get_processors(std::shared_ptr<AudioBuffer> buffer) const
{
    static const std::vector<std::shared_ptr<BufferProcessor>> empty_vector;

    auto it = m_buffer_processors.find(buffer);
    if (it != m_buffer_processors.end()) {
        return it->second;
    }

    return empty_vector;
}
}
