#pragma once

#include "AudioBuffer.hpp"
#include "BufferProcessor.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

class NodeSourceProcessor : public BufferProcessor {
public:
    NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix = 0.5f, bool clear_before_process = false);

    void process(std::shared_ptr<AudioBuffer> buffer) override;

    inline void set_mix(float mix) { m_mix = mix; }

    inline float get_mix() const { return m_mix; }

private:
    std::shared_ptr<Nodes::Node> m_node;
    float m_mix;
    bool m_clear_before_process;
};

class NodeBuffer : public StandardAudioBuffer {

public:
    NodeBuffer(u_int32_t channel_id, u_int32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process = false);

    inline void set_clear_before_process(bool value) { m_clear_before_process = value; }
    inline bool get_clear_before_process() const { return m_clear_before_process; }

    void process_default() override;

protected:
    std::shared_ptr<BufferProcessor> create_default_processor() override;

    std::shared_ptr<BufferProcessor> m_default_processor;

private:
    std::shared_ptr<Nodes::Node> m_source_node;
    bool m_clear_before_process;
};
}
