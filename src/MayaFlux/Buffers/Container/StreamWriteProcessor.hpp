#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Kakshya {
class DynamicSoundStream;
}

namespace MayaFlux::Buffers {

/**
 * @class StreamWriteProcessor
 * @brief Minimal processor that writes AudioBuffer data to DynamicSoundStream
 *
 * The ::process() call does the actual work - just calls container->write_frames()
 */
class StreamWriteProcessor : public BufferProcessor {
public:
    explicit StreamWriteProcessor(std::shared_ptr<Kakshya::DynamicSoundStream> container)
        : m_container(container)
    {
    }

    void processing_function(std::shared_ptr<Buffer> buffer) override;

    inline std::shared_ptr<Kakshya::DynamicSoundStream> get_container() const { return m_container; }

private:
    std::shared_ptr<Kakshya::DynamicSoundStream> m_container;
};

} // namespace MayaFlux::Buffers
