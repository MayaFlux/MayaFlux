#pragma once

#include "AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Buffers {

class RootAudioBuffer : public StandardAudioBuffer {
public:
    RootAudioBuffer(u_int32_t channel_id, u_int32_t num_samples = 512);

    ~RootAudioBuffer() override = default;

    void add_child_buffer(std::shared_ptr<AudioBuffer> buffer);
    void remove_child_buffer(std::shared_ptr<AudioBuffer> buffer);

    const std::vector<std::shared_ptr<AudioBuffer>>& get_child_buffers() const;

    virtual void process_default() override;

    virtual void clear() override;

    virtual void resize(u_int32_t num_samples) override;

    void set_node_output(const std::vector<double>& data);

    inline const std::vector<double>& get_node_output() const { return m_node_output; }

    inline bool has_node_output() const { return m_has_node_output; }

    inline size_t get_num_children() const { return m_child_buffers.size(); }

protected:
    std::vector<std::shared_ptr<AudioBuffer>> m_child_buffers;

    virtual std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    std::vector<double> m_node_output;
    bool m_has_node_output;
    std::mutex m_mutex;
};

class ChannelProcessor : public BufferProcessor {
public:
    ChannelProcessor(RootAudioBuffer* root_buffer);

    void process(std::shared_ptr<AudioBuffer> buffer) override;

private:
    RootAudioBuffer* m_root_buffer;
};

class FinalLimiterProcessor : public Buffers::BufferProcessor {
public:
    void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override;
};

}
