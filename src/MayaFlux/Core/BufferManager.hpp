#pragma once

#include "MayaFlux/Buffers/RootAudioBuffer.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Core {

using AudioProcessingFunction = std::function<void(std::shared_ptr<Buffers::AudioBuffer>)>;

class BufferManager {
public:
    BufferManager(u_int32_t num_channels, u_int32_t num_frames);
    ~BufferManager() = default;

    inline u_int32_t get_num_channels() const { return m_num_channels; }
    inline u_int32_t get_num_frames() const { return m_num_frames; }

    std::shared_ptr<Buffers::AudioBuffer> get_channel(u_int32_t channel_index);
    const std::shared_ptr<Buffers::AudioBuffer> get_channel(u_int32_t channel_index) const;

    void add_buffer_to_channel(u_int32_t channel_index, std::shared_ptr<Buffers::AudioBuffer> buffer);
    void remove_buffer_from_channel(u_int32_t channel_index, std::shared_ptr<Buffers::AudioBuffer> buffer);
    const std::vector<std::shared_ptr<Buffers::AudioBuffer>>& get_channel_buffers(u_int32_t channel_index) const;

    std::vector<double>& get_channel_data(u_int32_t channel_index);
    const std::vector<double>& get_channel_data(u_int32_t channel_index) const;

    void process_channel(u_int32_t channel_index);
    void process_all_channels();

    std::shared_ptr<Buffers::BufferProcessingChain> get_channel_processing_chain(u_int32_t channel_index);
    inline std::shared_ptr<Buffers::BufferProcessingChain> get_global_processing_chain() { return m_global_processing_chain; }

    void add_processor(std::shared_ptr<Buffers::BufferProcessor> processor, std::shared_ptr<Buffers::AudioBuffer> buffer);
    void add_processor_to_channel(std::shared_ptr<Buffers::BufferProcessor> processor, u_int32_t channel_index);
    void add_processor_to_all(std::shared_ptr<Buffers::BufferProcessor> processor);

    void remove_processor(std::shared_ptr<Buffers::BufferProcessor> processor, std::shared_ptr<Buffers::AudioBuffer> buffer);
    void remove_processor_from_channel(std::shared_ptr<Buffers::BufferProcessor> processor, u_int32_t channel_index);
    void remove_processor_from_all(std::shared_ptr<Buffers::BufferProcessor> processor);

    std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<Buffers::AudioBuffer> buffer);
    std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_channel(AudioProcessingFunction processor, u_int32_t channel_index);
    std::shared_ptr<Buffers::BufferProcessor> attach_quick_process_to_all(AudioProcessingFunction processor);

    void set_final_processor_for_root_buffers(std::shared_ptr<Buffers::BufferProcessor> processor);

    void connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix = 0.5, bool clear_before = false);
    void connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<Buffers::AudioBuffer>, float mix = 0.5, bool clear_before = true);

    template <typename BufferType, typename... Args>
    std::shared_ptr<BufferType> create_specialized_buffer(u_int32_t channel_index, Args&&... args)
    {
        if (channel_index >= m_num_channels) {
            throw std::out_of_range("Channel index out of range");
        }

        auto buffer = std::make_shared<BufferType>(channel_index, m_num_frames, std::forward<Args>(args)...);
        buffer->set_processing_chain(m_channel_processing_chains[channel_index]);
        m_root_buffers[channel_index]->add_child_buffer(buffer);

        return buffer;
    }

    void fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames);
    void fill_interleaved(double* interleaved_data, u_int32_t num_frames) const;

    void resize(u_int32_t num_frames);

private:
    u_int32_t m_num_channels;
    u_int32_t m_num_frames;

    std::vector<std::shared_ptr<Buffers::RootAudioBuffer>> m_root_buffers;

    std::vector<std::shared_ptr<Buffers::BufferProcessingChain>> m_channel_processing_chains;

    std::shared_ptr<Buffers::BufferProcessingChain> m_global_processing_chain;
};

}
