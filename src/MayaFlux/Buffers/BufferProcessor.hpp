#pragma once

#include "config.h"

namespace MayaFlux::Buffers {

class AudioBuffer;

class BufferProcessor {
public:
    virtual ~BufferProcessor() = default;

    virtual void process(std::shared_ptr<AudioBuffer> buffer) = 0;

    virtual void on_attach(std::shared_ptr<AudioBuffer> buffer) { };
    virtual void on_detach(std::shared_ptr<AudioBuffer> buffer) { };
};

class BufferProcessingChain {
public:
    void add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    void remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer);

    void process(std::shared_ptr<AudioBuffer> buffer);

    bool has_processors(std::shared_ptr<AudioBuffer> buffer) const;

    const std::vector<std::shared_ptr<BufferProcessor>>& get_processors(std::shared_ptr<AudioBuffer> buffer) const;

    inline const std::unordered_map<std::shared_ptr<AudioBuffer>, std::vector<std::shared_ptr<BufferProcessor>>> get_chain() const { return m_buffer_processors; }

    void merge_chain(const std::shared_ptr<BufferProcessingChain> other);

private:
    std::unordered_map<std::shared_ptr<AudioBuffer>, std::vector<std::shared_ptr<BufferProcessor>>> m_buffer_processors;
};

}
