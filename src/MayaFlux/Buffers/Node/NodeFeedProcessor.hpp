#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Nodes {
class StreamReaderNode;
}

namespace MayaFlux::Buffers {

/**
 * @class NodeFeedProcessor
 * @brief Processor that feeds AudioBuffer data into a StreamReaderNode
 *
 * Attach to any AudioBuffer's processing chain. Each cycle it calls
 * set_data() on the target StreamReaderNode with the buffer's contents.
 * The feeder claims ownership on first write; only one feeder can
 * write to a given StreamReaderNode at a time. Setting a new target
 * releases the previous one.
 */
class MAYAFLUX_API NodeFeedProcessor : public BufferProcessor {
public:
    explicit NodeFeedProcessor(std::shared_ptr<Nodes::StreamReaderNode> target);

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    void set_target(std::shared_ptr<Nodes::StreamReaderNode> target);

private:
    std::shared_ptr<Nodes::StreamReaderNode> m_target;
};

}
