#include "NetworkAudioBuffer.hpp"

#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkAudioProcessor::NetworkAudioProcessor(
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    float mix,
    bool clear_before_process)
    : m_network(std::move(network))
    , m_mix(mix)
    , m_clear_before_process(clear_before_process)
{
}

void NetworkAudioProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
}

void NetworkAudioProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
}

void NetworkAudioProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_network) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NetworkAudioSourceProcessor has null network");
        return;
    }

    auto output_mode = m_network->get_output_mode();
    if (output_mode != Nodes::Network::OutputMode::AUDIO_SINK
        && output_mode != Nodes::Network::OutputMode::AUDIO_COMPUTE) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NetworkAudioSourceProcessor: network is not an audio sink (mode={})",
            static_cast<int>(output_mode));
        return;
    }

    if (!m_network->is_enabled()) {
        MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NetworkAudioSourceProcessor: network is disabled");
        return;
    }

    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer) {
        return;
    }

    bool should_clear = m_clear_before_process;
    if (auto net_buf = std::dynamic_pointer_cast<NetworkAudioBuffer>(buffer)) {
        should_clear = net_buf->get_clear_before_process();
    }

    if (should_clear) {
        buffer->clear();
    }

    if (!m_network->is_processed_this_cycle()) {
        if (m_network->is_processing()) {
            return;
        }

        m_self_processing = true;
        m_network->add_channel_usage(audio_buffer->get_channel_id());
        m_network->mark_processing(true);
        m_network->process_batch(audio_buffer->get_num_samples());
        m_network->mark_processing(false);
        m_network->mark_processed(true);
    }

    const auto net_output = m_network->get_audio_buffer();
    if (!net_output) {
        return;
    }

    auto& buffer_data = audio_buffer->get_data();
    const auto& network_data = *net_output;

    auto scale = static_cast<double>(m_mix);

    if (m_self_processing) {
        if (m_network->needs_channel_routing()) {
            uint32_t channel = audio_buffer->get_channel_id();
            double routing_amount = m_network->get_routing_state().amount[channel];
            scale *= routing_amount;
        }
    }

    if (scale == 0.0) {
        return;
    }

    const size_t copy_size = std::min(buffer_data.size(), network_data.size());

    if (should_clear && scale == 1.0) {
        std::copy_n(network_data.begin(), copy_size, buffer_data.begin());
    } else {
        for (size_t i = 0; i < copy_size; ++i) {
            buffer_data[i] += network_data[i] * scale;
        }
    }

    if (m_self_processing) {
        m_self_processing = false;
        m_network->request_reset_from_channel(audio_buffer->get_channel_id());
    }
}

NetworkAudioBuffer::NetworkAudioBuffer(
    uint32_t channel_id,
    uint32_t num_samples,
    std::shared_ptr<Nodes::Network::NodeNetwork> network,
    bool clear_before_process)
    : AudioBuffer(channel_id, num_samples)
    , m_network(std::move(network))
    , m_clear_before_process(clear_before_process)
{
    auto mode = m_network->get_output_mode();
    if (mode != Nodes::Network::OutputMode::AUDIO_SINK
        && mode != Nodes::Network::OutputMode::AUDIO_COMPUTE) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "NetworkAudioBuffer requires AUDIO_SINK or AUDIO_COMPUTE network, got mode={}",
            static_cast<int>(mode));
    }
}

void NetworkAudioBuffer::setup_processors(ProcessingToken /*token*/)
{
    m_default_processor = create_default_processor();
    m_default_processor->on_attach(shared_from_this());
}

void NetworkAudioBuffer::process_default()
{
    if (m_clear_before_process) {
        clear();
    }
    m_default_processor->process(shared_from_this());
}

std::shared_ptr<BufferProcessor> NetworkAudioBuffer::create_default_processor()
{
    return std::make_shared<NetworkAudioProcessor>(m_network);
}

}
