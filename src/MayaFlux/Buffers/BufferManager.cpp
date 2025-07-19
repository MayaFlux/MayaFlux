#include "BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {

class QuickProcess : public BufferProcessor {
public:
    QuickProcess(AudioProcessingFunction function)
        : m_function(function)
    {
        m_processing_token = ProcessingToken::AUDIO_BACKEND;
    }

    // For now hardcoded to AUDIO_BACKEND, but can be extended later
    void processing_function(std::shared_ptr<Buffer> buffer) override
    {
        auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
        if (audio_buffer) {
            m_function(audio_buffer);
        }
    }

    void on_attach(std::shared_ptr<Buffer> buffer) override
    {
        auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
        if (!audio_buffer) {
            throw std::runtime_error("QuickProcess can only be attached to AudioBuffer");
        }
    }

    bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override
    {
        return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
    }

private:
    AudioProcessingFunction m_function;
};

BufferManager::BufferManager(u_int32_t default_channels, u_int32_t default_buffer_size, ProcessingToken default_processing_token)
    : m_default_token(default_processing_token)
    , m_global_processing_chain(std::make_shared<BufferProcessingChain>())
{
    m_token_channel_counts[default_processing_token] = default_channels;
    m_token_buffer_sizes[default_processing_token] = default_buffer_size;

    auto& root_buffers = m_token_root_buffers[default_processing_token];
    auto& processing_chains = m_token_processing_chains[default_processing_token];

    root_buffers.reserve(default_channels);
    processing_chains.reserve(default_channels);

    for (u_int32_t i = 0; i < default_channels; i++) {
        auto root_buffer = create_root_audio_buffer(default_processing_token, i, default_buffer_size);
        root_buffers.push_back(root_buffer);

        auto processing_chain = std::make_shared<BufferProcessingChain>();
        processing_chain->set_preferred_token(default_processing_token);
        processing_chain->set_enforcement_strategy(TokenEnforcementStrategy::FILTERED);
        processing_chains.push_back(processing_chain);

        root_buffer->set_processing_chain(processing_chain);
    }

    if (default_processing_token == ProcessingToken::AUDIO_BACKEND || default_processing_token == ProcessingToken::AUDIO_PARALLEL) {
        auto limiter = std::make_shared<FinalLimiterProcessor>();
        set_final_processor(limiter, default_processing_token);
    }
}

void BufferManager::process_token(ProcessingToken token, u_int32_t processing_units)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end()) {
        return;
    }

    auto& root_buffers = token_it->second;

    auto processor_it = m_token_processors.find(token);
    if (processor_it != m_token_processors.end()) {
        processor_it->second(root_buffers, processing_units);
        return;
    }

    for (u_int32_t channel = 0; channel < root_buffers.size(); channel++) {
        process_channel(token, channel, processing_units);
    }
}

void BufferManager::process_all_tokens()
{
    for (const auto& [token, root_buffers] : m_token_root_buffers) {
        if (!root_buffers.empty()) {
            u_int32_t processing_units = m_token_buffer_sizes[token];
            process_token(token, processing_units);
        }
    }
}

void BufferManager::process_channel(
    ProcessingToken token,
    u_int32_t channel,
    u_int32_t processing_units,
    const std::vector<double>& node_output_data)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end() || channel >= token_it->second.size()) {
        return;
    }

    auto& root_buffer = token_it->second[channel];
    auto& processing_chains = m_token_processing_chains[token];

    if (!node_output_data.empty()) {
        root_buffer->set_node_output(node_output_data);
    }

    for (auto& child : root_buffer->get_child_buffers()) {
        if (child->needs_default_processing()) {
            child->process_default();
        }

        if (auto processing_chain = child->get_processing_chain()) {
            if (child->has_data_for_cycle()) {
                processing_chain->process(child);
            }
        }
    }

    root_buffer->process_default();

    if (channel < processing_chains.size()) {
        processing_chains[channel]->process(root_buffer);
    }

    m_global_processing_chain->process(root_buffer);

    if (auto chain = root_buffer->get_processing_chain()) {
        chain->process_final(root_buffer);
    }
}

std::vector<ProcessingToken> BufferManager::get_active_tokens() const
{
    std::vector<ProcessingToken> active_tokens;
    for (const auto& [token, root_buffers] : m_token_root_buffers) {
        if (!root_buffers.empty()) {
            active_tokens.push_back(token);
        }
    }
    return active_tokens;
}

void BufferManager::register_token_processor(ProcessingToken token,
    std::function<void(std::vector<std::shared_ptr<RootAudioBuffer>>&, u_int32_t)> processor)
{
    m_token_processors[token] = processor;
}

std::shared_ptr<RootAudioBuffer> BufferManager::get_root_audio_buffer(ProcessingToken token, u_int32_t channel)
{
    ensure_channel_exists(token, channel);
    return m_token_root_buffers[token][channel];
}

std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, u_int32_t channel)
{
    return get_root_audio_buffer(token, channel)->get_data();
}

const std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, u_int32_t channel) const
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end() || channel >= token_it->second.size()) {
        throw std::out_of_range("Token/channel combination out of range");
    }

    return token_it->second[channel]->get_data();
}

u_int32_t BufferManager::get_num_channels(ProcessingToken token) const
{
    auto it = m_token_channel_counts.find(token);
    return (it != m_token_channel_counts.end()) ? it->second : 0;
}

u_int32_t BufferManager::get_root_audio_buffer_size(ProcessingToken token) const
{
    auto it = m_token_buffer_sizes.find(token);
    return (it != m_token_buffer_sizes.end()) ? it->second : 512;
}

void BufferManager::set_root_audio_buffer_size(ProcessingToken token, u_int32_t buffer_size)
{
    std::lock_guard<std::mutex> lock(m_manager_mutex);

    m_token_buffer_sizes[token] = buffer_size;

    auto token_it = m_token_root_buffers.find(token);
    if (token_it != m_token_root_buffers.end()) {
        for (auto& root_buffer : token_it->second) {
            root_buffer->resize(buffer_size);
        }
    }
}

void BufferManager::add_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, u_int32_t channel)
{
    ensure_channel_exists(token, channel);

    buffer->set_channel_id(channel);

    auto& processing_chains = m_token_processing_chains[token];
    if (auto buf_chain = buffer->get_processing_chain()) {
        if (buf_chain != processing_chains[channel]) {
            processing_chains[channel]->merge_chain(buf_chain);
        }
    } else {
        buffer->set_processing_chain(processing_chains[channel]);
    }

    m_token_root_buffers[token][channel]->add_child_buffer(buffer);
}

void BufferManager::remove_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, u_int32_t channel)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end() || channel >= token_it->second.size()) {
        return;
    }

    token_it->second[channel]->remove_child_buffer(buffer);
}

const std::vector<std::shared_ptr<AudioBuffer>>& BufferManager::get_audio_buffers(ProcessingToken token, u_int32_t channel) const
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end() || channel >= token_it->second.size()) {
        throw std::out_of_range("Token/channel combination out of range");
    }

    return token_it->second[channel]->get_child_buffers();
}

std::shared_ptr<BufferProcessingChain> BufferManager::get_processing_chain(ProcessingToken token, u_int32_t channel)
{
    ensure_channel_exists(token, channel);
    return m_token_processing_chains[token][channel];
}

void BufferManager::add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    u_int32_t channel_id = buffer->get_channel_id();

    for (const auto& [token, root_buffers] : m_token_root_buffers) {
        if (channel_id < root_buffers.size()) {
            auto& processing_chains = m_token_processing_chains[token];
            if (channel_id < processing_chains.size()) {
                processing_chains[channel_id]->add_processor(processor, buffer);
                buffer->set_processing_chain(processing_chains[channel_id]);
                return;
            }
        }
    }

    m_global_processing_chain->add_processor(processor, buffer);
}

void BufferManager::add_processor_to_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, u_int32_t channel)
{
    ensure_channel_exists(token, channel);

    auto& root_buffer = m_token_root_buffers[token][channel];
    auto& processing_chain = m_token_processing_chains[token][channel];

    processing_chain->add_processor(processor, root_buffer);
}

void BufferManager::add_processor_to_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end()) {
        return;
    }

    auto& root_buffers = token_it->second;
    auto& processing_chains = m_token_processing_chains[token];

    for (size_t i = 0; i < root_buffers.size() && i < processing_chains.size(); i++) {
        processing_chains[i]->add_processor(processor, root_buffers[i]);
    }
}

void BufferManager::remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    for (const auto& [token, processing_chains] : m_token_processing_chains) {
        for (auto& chain : processing_chains) {
            chain->remove_processor(processor, buffer);
        }
    }

    m_global_processing_chain->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, u_int32_t channel)
{
    auto token_it = m_token_processing_chains.find(token);
    if (token_it == m_token_processing_chains.end() || channel >= token_it->second.size()) {
        return;
    }

    auto& root_buffer = m_token_root_buffers[token][channel];
    token_it->second[channel]->remove_processor(processor, root_buffer);
}

void BufferManager::remove_processor_from_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto token_it = m_token_processing_chains.find(token);
    if (token_it == m_token_processing_chains.end()) {
        return;
    }

    auto& root_buffers = m_token_root_buffers[token];
    auto& processing_chains = token_it->second;

    for (size_t i = 0; i < processing_chains.size() && i < root_buffers.size(); i++) {
        processing_chains[i]->remove_processor(processor, root_buffers[i]);
    }
}

void BufferManager::set_final_processor(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto token_it = m_token_processing_chains.find(token);
    if (token_it == m_token_processing_chains.end()) {
        return;
    }

    auto& root_buffers = m_token_root_buffers[token];
    auto& processing_chains = token_it->second;

    for (size_t i = 0; i < processing_chains.size() && i < root_buffers.size(); i++) {
        processing_chains[i]->add_final_processor(processor, root_buffers[i]);
    }
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<AudioBuffer> buffer)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor(quick_process, buffer);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process_to_channel(AudioProcessingFunction processor, ProcessingToken token, u_int32_t channel)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor_to_channel(quick_process, token, channel);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process_to_token(AudioProcessingFunction processor, ProcessingToken token)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor_to_token(quick_process, token);
    return quick_process;
}

void BufferManager::connect_node_to_channel(std::shared_ptr<Nodes::Node> node, ProcessingToken token, u_int32_t channel, float mix, bool clear_before)
{
    // TODO: Implement NodeSourceProcessor for generic Buffer interface
    // For now, placeholder that shows the integration pattern

    ensure_channel_exists(token, channel);

    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);

    processor->set_processing_token(token);

    add_processor_to_channel(processor, token, channel);
}

void BufferManager::connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<AudioBuffer> buffer, float mix, bool clear_before)
{
    // TODO: Implement NodeSourceProcessor for generic Buffer interface
    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);
    add_processor(processor, buffer);
}

void BufferManager::fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames, ProcessingToken token, u_int32_t num_channels)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end()) {
        return;
    }

    auto& root_buffers = token_it->second;
    u_int32_t channels_to_process = std::min(num_channels, static_cast<u_int32_t>(root_buffers.size()));

    for (u_int32_t channel = 0; channel < channels_to_process; channel++) {
        auto& data = root_buffers[channel]->get_data();
        u_int32_t frames_to_copy = std::min(num_frames, static_cast<u_int32_t>(data.size()));

        for (u_int32_t frame = 0; frame < frames_to_copy; frame++) {
            data[frame] = interleaved_data[frame * num_channels + channel];
        }
    }
}

void BufferManager::fill_interleaved(double* interleaved_data, u_int32_t num_frames, ProcessingToken token, u_int32_t num_channels) const
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end()) {
        return;
    }

    const auto& root_buffers = token_it->second;
    u_int32_t channels_to_process = std::min(num_channels, static_cast<u_int32_t>(root_buffers.size()));

    for (u_int32_t channel = 0; channel < channels_to_process; channel++) {
        const auto& data = root_buffers[channel]->get_data();
        u_int32_t frames_to_copy = std::min(num_frames, static_cast<u_int32_t>(data.size()));

        for (u_int32_t frame = 0; frame < frames_to_copy; frame++) {
            interleaved_data[frame * num_channels + channel] = data[frame];
        }
    }
}

void BufferManager::resize_root_buffers(ProcessingToken token, u_int32_t buffer_size)
{
    set_root_audio_buffer_size(token, buffer_size);
}

void BufferManager::ensure_channel_exists(ProcessingToken token, u_int32_t channel)
{
    auto token_it = m_token_root_buffers.find(token);
    if (token_it == m_token_root_buffers.end()) {
        u_int32_t default_channels = (token == m_default_token) ? m_token_channel_counts[token] : 2;
        u_int32_t default_buffer_size = (token == m_default_token) ? m_token_buffer_sizes[token] : 512;

        m_token_channel_counts[token] = std::max(default_channels, channel + 1);
        m_token_buffer_sizes[token] = default_buffer_size;

        auto& root_buffers = m_token_root_buffers[token];
        auto& processing_chains = m_token_processing_chains[token];

        root_buffers.resize(channel + 1);
        processing_chains.resize(channel + 1);

        for (u_int32_t i = 0; i <= channel; i++) {
            if (!root_buffers[i]) {
                root_buffers[i] = create_root_audio_buffer(token, i, default_buffer_size);
                processing_chains[i] = std::make_shared<BufferProcessingChain>();
                processing_chains[i]->set_preferred_token(token);
                processing_chains[i]->set_enforcement_strategy(TokenEnforcementStrategy::FILTERED);
                root_buffers[i]->set_processing_chain(processing_chains[i]);
            }
        }
    } else {
        auto& root_buffers = token_it->second;
        auto& processing_chains = m_token_processing_chains[token];

        if (channel >= root_buffers.size()) {
            u_int32_t old_size = root_buffers.size();
            u_int32_t new_size = channel + 1;

            root_buffers.resize(new_size);
            processing_chains.resize(new_size);
            m_token_channel_counts[token] = new_size;

            u_int32_t buffer_size = m_token_buffer_sizes[token];

            for (u_int32_t i = old_size; i < new_size; i++) {
                root_buffers[i] = create_root_audio_buffer(token, i, buffer_size);
                processing_chains[i] = std::make_shared<BufferProcessingChain>();
                processing_chains[i]->set_preferred_token(token);
                processing_chains[i]->set_enforcement_strategy(TokenEnforcementStrategy::FILTERED);
                root_buffers[i]->set_processing_chain(processing_chains[i]);
            }
        }
    }
}

std::shared_ptr<RootAudioBuffer> BufferManager::create_root_audio_buffer(ProcessingToken token, u_int32_t channel, u_int32_t buffer_size)
{
    // FAKE
    auto root_buffer = std::make_shared<RootAudioBuffer>(channel, buffer_size);
    root_buffer->initialize();
    root_buffer->set_token_active(true);
    return root_buffer;
}

} // namespace MayaFlux::Buffers
