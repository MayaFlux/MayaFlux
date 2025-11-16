#include "RootAudioBuffer.hpp"

namespace MayaFlux::Buffers {

ChannelProcessor::ChannelProcessor(std::shared_ptr<Buffer> root_buffer)
    : m_root_buffer(std::dynamic_pointer_cast<RootAudioBuffer>(root_buffer))
{
    m_processing_token = ProcessingToken::AUDIO_BACKEND;
}

void ChannelProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{

    auto root_audio_buffer = std::dynamic_pointer_cast<RootAudioBuffer>(buffer);
    if (!root_audio_buffer || root_audio_buffer != m_root_buffer) {
        return;
    }

    auto& output_data = root_audio_buffer->get_data();
    const size_t buffer_size = output_data.size();
    std::ranges::fill(output_data, 0.0);

    if (root_audio_buffer->has_node_output()) {
        const auto& node_output = root_audio_buffer->get_node_output();
        const size_t node_size = node_output.size();
        const size_t add_size = std::min(buffer_size, node_size);

        for (size_t i = 0; i < add_size; ++i) {
            output_data[i] += node_output[i];
        }
    }

    std::vector<std::shared_ptr<AudioBuffer>> buffers_to_remove;
    for (auto& child : m_root_buffer->get_child_buffers()) {
        if (child->needs_removal()) {
            buffers_to_remove.push_back(child);
        }
    }

    uint32_t active_buffers = 0;
    for (auto& child : m_root_buffer->get_child_buffers()) {
        if (child->has_data_for_cycle() && !child->needs_removal() && !child->is_internal_only()) {
            active_buffers++;
        }
    }

    if (active_buffers > 0) {
        for (auto& child : m_root_buffer->get_child_buffers()) {
            if (child->has_data_for_cycle() && !child->needs_removal() && !child->is_internal_only()) {
                const auto& child_data = child->get_data();
                for (size_t i = 0; i < std::min(child_data.size(), output_data.size()); i++) {
                    output_data[i] += child_data[i] / active_buffers;
                }
            }
        }
    }

    for (auto& child : buffers_to_remove) {
        m_root_buffer->remove_child_buffer(child);
    }
}

void ChannelProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    auto root_audio_buffer = std::dynamic_pointer_cast<RootAudioBuffer>(buffer);
    if (!root_audio_buffer) {
        throw std::runtime_error("ChannelProcessor can only be attached to RootAudioBuffer");
    }

    if (!are_tokens_compatible(ProcessingToken::AUDIO_BACKEND, m_processing_token)) {
        throw std::runtime_error("ChannelProcessor token incompatible with RootAudioBuffer requirements");
    }
}

bool ChannelProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    auto root_audio_buffer = std::dynamic_pointer_cast<RootAudioBuffer>(buffer);
    return root_audio_buffer != nullptr;
}

RootAudioBuffer::RootAudioBuffer(uint32_t channel_id, uint32_t num_samples)
    : RootBuffer<AudioBuffer>(channel_id, num_samples)
    , m_has_node_output(false)
{
    m_preferred_processing_token = ProcessingToken::AUDIO_BACKEND;
    m_token_enforcement_strategy = TokenEnforcementStrategy::STRICT;
}

void RootAudioBuffer::initialize()
{
    auto channel_processor = create_default_processor();
    if (channel_processor) {
        set_default_processor(channel_processor);
    }
}

void RootAudioBuffer::set_node_output(const std::vector<double>& data)
{
    if (m_node_output.size() != data.size()) {
        m_node_output.resize(data.size());
    }
    std::ranges::copy(data, m_node_output.begin());
    m_has_node_output = true;
}

void RootAudioBuffer::process_default()
{
    if (this->has_pending_operations()) {
        this->process_pending_buffer_operations();
    }

    if (m_default_processor && m_process_default) {
        m_default_processor->process(shared_from_this());
    }
}

void RootAudioBuffer::resize(uint32_t num_samples)
{
    AudioBuffer::resize(num_samples);

    m_node_output.resize(num_samples, 0.0);

    for (auto& child : m_child_buffers) {
        if (child) {
            child->resize(num_samples);
        }
    }
}

std::shared_ptr<BufferProcessor> RootAudioBuffer::create_default_processor()
{
    return std::make_shared<ChannelProcessor>(shared_from_this());
}

FinalLimiterProcessor::FinalLimiterProcessor()
{
    m_processing_token = ProcessingToken::AUDIO_BACKEND;
}

void FinalLimiterProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer) {
        throw std::runtime_error("FinalLimiterProcessor can only be attached to AudioBuffer-derived types");
    }

    if (!are_tokens_compatible(ProcessingToken::AUDIO_BACKEND, m_processing_token)) {
        throw std::runtime_error("FinalLimiterProcessor token incompatible with audio processing requirements");
    }
}

bool FinalLimiterProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

void FinalLimiterProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer) {
        return;
    }

    auto& data = audio_buffer->get_data();

    constexpr double threshold = 0.95;
    constexpr double knee = 0.1;

    for (double& sample : data) {
        const double abs_sample = std::abs(sample);

        if (abs_sample > threshold) {
            const double excess = abs_sample - threshold;
            const double compressed_excess = std::tanh(excess / knee) * knee;
            const double limited_abs = threshold + compressed_excess;

            sample = (sample >= 0.0) ? limited_abs : -limited_abs;
        }
    }
}
}
