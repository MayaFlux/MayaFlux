#include "BufferManager.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Buffers/Root/MixProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "Input/InputAudioBuffer.hpp"

#include "VKBuffer.hpp"

namespace MayaFlux::Buffers {

class QuickProcess : public BufferProcessor {
public:
    QuickProcess(AudioProcessingFunction function)
        : m_function(std::move(function))
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

    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override
    {
        return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
    }

private:
    AudioProcessingFunction m_function;
};

void RootAudioUnit::resize_channels(uint32_t new_count, uint32_t new_buffer_size, ProcessingToken token)
{
    if (new_count <= channel_count)
        return;

    uint32_t old_count = channel_count;
    channel_count = new_count;
    buffer_size = new_buffer_size;

    root_buffers.resize(new_count);
    processing_chains.resize(new_count);

    for (uint32_t i = old_count; i < new_count; ++i) {
        root_buffers[i] = std::make_shared<RootAudioBuffer>(i, buffer_size);
        root_buffers[i]->initialize();
        root_buffers[i]->set_token_active(true);

        processing_chains[i] = std::make_shared<BufferProcessingChain>();
        processing_chains[i]->set_preferred_token(token);
        processing_chains[i]->set_enforcement_strategy(TokenEnforcementStrategy::FILTERED);

        root_buffers[i]->set_processing_chain(processing_chains[i]);
    }
}

void RootAudioUnit::resize_buffers(uint32_t new_buffer_size)
{
    buffer_size = new_buffer_size;
    for (auto& buffer : root_buffers) {
        buffer->resize(new_buffer_size);
    }
}

BufferManager::BufferManager(uint32_t default_out_channels, uint32_t default_in_channels, uint32_t default_buffer_size, ProcessingToken default_processing_token)
    : m_default_token(default_processing_token)
    , m_global_processing_chain(std::make_shared<BufferProcessingChain>())
{
    ensure_channels(default_processing_token, default_out_channels);
    resize_root_audio_buffers(default_processing_token, default_buffer_size);

    if (default_in_channels) {
        setup_input_buffers(default_in_channels, default_buffer_size);
    }

    if (default_processing_token == ProcessingToken::AUDIO_BACKEND || default_processing_token == ProcessingToken::AUDIO_PARALLEL) {
        auto limiter = std::make_shared<FinalLimiterProcessor>();
        set_final_processor(limiter, default_processing_token);
    }
}

void BufferManager::process_token(ProcessingToken token, uint32_t processing_units)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end())
        return;

    auto& unit = it->second;

    if (unit.custom_processor) {
        unit.custom_processor(unit.root_buffers, processing_units);
        return;
    }

    for (uint32_t channel = 0; channel < unit.channel_count; ++channel) {
        process_channel(token, channel, processing_units);
    }
}

void BufferManager::process_all_tokens()
{
    for (const auto& [token, unit] : m_audio_units) {
        if (!unit.root_buffers.empty()) {
            process_token(token, unit.buffer_size);
        }
    }
}

void BufferManager::process_channel(
    ProcessingToken token,
    uint32_t channel,
    uint32_t processing_units,
    const std::vector<double>& node_output_data)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count)
        return;

    auto& unit = it->second;
    auto root_buffer = unit.get_buffer(channel);

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

    unit.get_chain(channel)->process(root_buffer);

    m_global_processing_chain->process(root_buffer);

    if (auto chain = root_buffer->get_processing_chain()) {
        chain->process_final(root_buffer);
    }
}

std::vector<ProcessingToken> BufferManager::get_active_tokens() const
{
    std::vector<ProcessingToken> tokens;
    for (const auto& [token, unit] : m_audio_units) {
        if (!unit.root_buffers.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

void BufferManager::register_token_processor(ProcessingToken token, RootProcessingFunction processor)
{
    auto& unit = get_or_create_unit(token);
    unit.custom_processor = std::move(processor);
}

std::shared_ptr<RootAudioBuffer> BufferManager::get_root_audio_buffer(ProcessingToken token, uint32_t channel)
{
    auto& unit = ensure_and_get_unit(token, channel);
    return unit.get_buffer(channel);
}

std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, uint32_t channel)
{
    return get_root_audio_buffer(token, channel)->get_data();
}

const std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, uint32_t channel) const
{
    auto& unit = get_audio_unit(token);
    if (channel >= unit.channel_count) {
        throw std::out_of_range("Token/channel combination out of range");
    }
    return unit.get_buffer(channel)->get_data();
}

uint32_t BufferManager::get_num_channels(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    return (it != m_audio_units.end()) ? it->second.channel_count : 0;
}

uint32_t BufferManager::get_root_audio_buffer_size(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    return (it != m_audio_units.end()) ? it->second.buffer_size : 512;
}

void BufferManager::resize_root_audio_buffers(ProcessingToken token, uint32_t buffer_size)
{
    auto& unit = get_or_create_unit(token);
    std::lock_guard<std::mutex> lock(m_manager_mutex);
    unit.resize_buffers(buffer_size);
}

void BufferManager::add_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, uint32_t channel)
{
    auto& unit = ensure_and_get_unit(token, channel);
    auto processing_chain = unit.get_chain(channel);
    buffer->set_channel_id(channel);

    if (auto buf_chain = buffer->get_processing_chain()) {
        if (buf_chain != processing_chain) {
            processing_chain->merge_chain(buf_chain);
        }
    } else {
        buffer->set_processing_chain(processing_chain);
    }

    {
        if (buffer->get_num_samples() != unit.buffer_size) {
            std::cerr << "BufferManager: Resizing buffer to match unit size: " << unit.buffer_size << std::endl;
            std::lock_guard<std::mutex> lock(m_manager_mutex);
            buffer->resize(unit.buffer_size);
        }
    }

    unit.get_buffer(channel)->add_child_buffer(buffer);
}

void BufferManager::remove_audio_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, uint32_t channel)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count)
        return;

    it->second.get_buffer(channel)->remove_child_buffer(std::move(buffer));
}

const std::vector<std::shared_ptr<AudioBuffer>>& BufferManager::get_audio_buffers(ProcessingToken token, uint32_t channel) const
{

    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count) {
        throw std::out_of_range("Token/channel combination out of range");
    }
    return it->second.get_buffer(channel)->get_child_buffers();
}

void BufferManager::add_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    if (!std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement, std::source_location::current(),
            "Unsupported graphics buffer type for token {}",
            static_cast<int>(token));
    }

    auto hook_it = m_buffer_init_hooks.find(token);
    if (hook_it != m_buffer_init_hooks.end()) {
        try {
            hook_it->second(buffer);
        } catch (const std::exception& e) {
            error_rethrow(Journal::Component::Core, Journal::Context::BufferManagement, std::source_location::current(),
                "Buffer init hook failed for token {}: {}",
                static_cast<int>(token), e.what());
        }
    }
}

void BufferManager::remove_graphics_buffer(const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    if (!std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement, std::source_location::current(),
            "Unsupported graphics buffer type for token {}",
            static_cast<int>(token));
    }
    auto hook_it = m_buffer_cleanup_hooks.find(token);
    if (hook_it != m_buffer_cleanup_hooks.end()) {
        hook_it->second(buffer);
        m_buffer_cleanup_hooks.erase(hook_it);
    }
}

std::shared_ptr<BufferProcessingChain> BufferManager::get_processing_chain(ProcessingToken token, uint32_t channel)
{
    auto& unit = ensure_and_get_unit(token, channel);
    return unit.get_chain(channel);
}

void BufferManager::add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    uint32_t channel_id = buffer->get_channel_id();

    for (auto& [token, unit] : m_audio_units) {
        if (channel_id < unit.channel_count) {
            auto processing_chain = unit.get_chain(channel_id);
            processing_chain->add_processor(processor, buffer);
            buffer->set_processing_chain(processing_chain);
            return;
        }
    }

    m_global_processing_chain->add_processor(processor, buffer);
}

void BufferManager::add_processor_to_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, uint32_t channel)
{
    get_processing_chain(token, channel)->add_processor(processor, get_root_audio_buffer(token, channel));
}

void BufferManager::add_processor_to_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end())
        return;

    auto& unit = it->second;
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        unit.get_chain(i)->add_processor(processor, unit.get_buffer(i));
    }
}

void BufferManager::remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    uint32_t channel_id = buffer->get_channel_id();

    for (const auto& [token, unit] : m_audio_units) {
        if (channel_id < unit.channel_count) {
            auto processing_chain = unit.get_chain(channel_id);
            processing_chain->remove_processor(processor, buffer);
            if (auto buf_chain = buffer->get_processing_chain()) {
                buf_chain->remove_processor(processor, buffer);
            }
        }
    }

    m_global_processing_chain->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_channel(std::shared_ptr<BufferProcessor> processor, ProcessingToken token, uint32_t channel)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count) {
        return;
    }
    it->second.get_chain(channel)->remove_processor(processor, it->second.get_buffer(channel));
}

void BufferManager::remove_processor_from_token(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {
        return;
    }

    auto& unit = it->second;
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        unit.get_chain(i)->remove_processor(processor, unit.get_buffer(i));
    }
}

void BufferManager::set_final_processor(std::shared_ptr<BufferProcessor> processor, ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end())
        return;

    auto& unit = it->second;
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        unit.get_chain(i)->add_final_processor(processor, unit.get_buffer(i));
    }
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<AudioBuffer> buffer)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor(quick_process, buffer);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process_to_channel(AudioProcessingFunction processor, ProcessingToken token, uint32_t channel)
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

void BufferManager::connect_node_to_channel(std::shared_ptr<Nodes::Node> node, ProcessingToken token, uint32_t channel, float mix, bool clear_before)
{
    // TODO: Implement NodeSourceProcessor for generic Buffer interface
    // For now, placeholder that shows the integration pattern

    ensure_channels(token, channel + 1);

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

void BufferManager::fill_from_interleaved(const double* interleaved_data, uint32_t num_frames, ProcessingToken token, uint32_t num_channels)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end())
        return;

    auto& unit = it->second;
    uint32_t channels_to_process = std::min(num_channels, unit.channel_count);

    for (uint32_t channel = 0; channel < channels_to_process; ++channel) {
        auto& buffer_data = unit.get_buffer(channel)->get_data();
        uint32_t frames_to_copy = std::min(num_frames, static_cast<uint32_t>(buffer_data.size()));

        for (uint32_t frame = 0; frame < frames_to_copy; ++frame) {
            buffer_data[frame] = interleaved_data[frame * num_channels + channel];
        }
    }
}

void BufferManager::fill_interleaved(double* interleaved_data, uint32_t num_frames, ProcessingToken token, uint32_t num_channels) const
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end())
        return;

    const auto& unit = it->second;
    uint32_t channels_to_process = std::min(num_channels, unit.channel_count);

    for (uint32_t channel = 0; channel < channels_to_process; ++channel) {
        const auto& buffer_data = unit.get_buffer(channel)->get_data();
        uint32_t frames_to_copy = std::min(num_frames, static_cast<uint32_t>(buffer_data.size()));

        for (uint32_t frame = 0; frame < frames_to_copy; ++frame) {
            interleaved_data[frame * num_channels + channel] = buffer_data[frame];
        }
    }
}

void BufferManager::clone_buffer_for_channels(std::shared_ptr<AudioBuffer> buffer, std::vector<uint32_t> channels, ProcessingToken token)
{
    if (channels.empty()) {
        std::cerr << "BufferManager: No channels specified for cloning" << std::endl;
        return;
    }
    if (!buffer) {
        std::cerr << "BufferManager: Invalid buffer for cloning" << std::endl;
        return;
    }

    for (const auto channel : channels) {
        if (channel >= m_audio_units[token].channel_count) {
            std::cerr << "BufferManager: Channel " << channel << " out of range for cloning" << std::endl;
            return;
        }

        auto cloned_buffer = buffer->clone_to(channel);
        add_audio_buffer(cloned_buffer, token, channel);
    }
}

void BufferManager::process_input(double* input_data, uint32_t num_channels, uint32_t num_frames)
{
    if (m_input_buffers.empty() || m_input_buffers.size() < num_channels) {
        setup_input_buffers(num_channels, num_frames);
    }

    if (!input_data) {
        std::cerr << "BufferManager: Invalid input data pointer" << std::endl;
        return;
    }

    for (uint32_t i = 0; i < m_input_buffers.size(); ++i) {
        auto& data = m_input_buffers[i]->get_data();

        for (uint32_t frame = 0; frame < data.size(); ++frame) {
            data[frame] = static_cast<double*>(input_data)[frame * num_channels + i];
        }
        m_input_buffers[i]->process_default();
    }
}

RootAudioUnit& BufferManager::get_or_create_unit(ProcessingToken token)
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {

        std::lock_guard<std::mutex> lock(m_manager_mutex);
        auto [inserted_it, success] = m_audio_units.emplace(token, RootAudioUnit {});
        return inserted_it->second;
    }
    return it->second;
}

const RootAudioUnit& BufferManager::get_audio_unit(ProcessingToken token) const
{
    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end()) {
        throw std::out_of_range("Token not found");
    }
    return it->second;
}

void BufferManager::ensure_channels(ProcessingToken token, uint32_t channel_count)
{
    auto& unit = get_or_create_unit(token);
    if (channel_count > unit.channel_count) {
        std::lock_guard<std::mutex> lock(m_manager_mutex);
        unit.resize_channels(channel_count, unit.buffer_size, token);
    }
}

void BufferManager::setup_input_buffers(uint32_t default_in_channels, uint32_t default_buffer_size)
{
    for (uint32_t i = 0; i < default_in_channels; ++i) {
        auto input = std::make_shared<InputAudioBuffer>(i, default_buffer_size);
        auto processor = std::make_shared<InputAccessProcessor>();
        input->set_default_processor(processor);
        m_input_buffers.push_back(input);
    }
}

RootAudioUnit& BufferManager::ensure_and_get_unit(ProcessingToken token, uint32_t channel)
{
    auto& unit = get_or_create_unit(token);
    if (channel >= unit.channel_count) {
        ensure_channels(token, channel + 1);
    }
    return unit;
}

void BufferManager::register_input_listener(std::shared_ptr<AudioBuffer> buffer, uint32_t channel)
{
    if (channel >= m_input_buffers.size()) {
        std::cerr << "BufferManager: Input channel " << channel << " out of range" << std::endl;
        return;
    }

    auto input_buffer = m_input_buffers[channel];
    if (input_buffer) {
        input_buffer->register_listener(buffer);
    }
}

void BufferManager::unregister_input_listener(std::shared_ptr<AudioBuffer> buffer, uint32_t channel)
{
    if (channel >= m_input_buffers.size()) {
        return;
    }

    auto input_buffer = m_input_buffers[channel];
    if (input_buffer) {
        input_buffer->unregister_listener(buffer);
    }
}

bool BufferManager::supply_buffer_to(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, uint32_t channel, double mix)
{
    if (!buffer) {
        std::cerr << "BufferManager: Invalid buffer for supplying" << std::endl;
        return false;
    }

    if (buffer->get_channel_id() == channel) {
        std::cerr << "BufferManager: Buffer already has the correct channel ID" << std::endl;
        return false;
    }

    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count) {
        std::cerr << "BufferManager: Token/channel combination out of range for supplying" << std::endl;
        return false;
    }

    auto root_buffer = it->second.get_buffer(channel);
    auto processing_chain = it->second.get_chain(channel);

    std::shared_ptr<MixProcessor> mix_processor = processing_chain->get_processor<MixProcessor>(root_buffer);

    if (!mix_processor) {
        mix_processor = std::make_shared<MixProcessor>();
        processing_chain->add_processor(mix_processor, root_buffer);
    }

    return mix_processor->register_source(buffer, mix, false);
}

bool BufferManager::remove_supplied_buffer(std::shared_ptr<AudioBuffer> buffer, ProcessingToken token, uint32_t channel)
{
    if (!buffer) {
        std::cerr << "BufferManager: Invalid buffer for removal" << std::endl;
        return false;
    }

    auto it = m_audio_units.find(token);
    if (it == m_audio_units.end() || channel >= it->second.channel_count) {
        std::cerr << "BufferManager: Token/channel combination out of range for removal" << std::endl;
        return false;
    }

    auto root_buffer = it->second.get_buffer(channel);
    auto processing_chain = it->second.get_chain(channel);

    if (std::shared_ptr<MixProcessor> mix_processor = processing_chain->get_processor<MixProcessor>(root_buffer)) {
        return mix_processor->remove_source(buffer);
    }
    return false;
}

void BufferManager::register_buffer_init_hook(ProcessingToken token, const BufferInitCallback& callback)
{
    if (!callback) {
        MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
            "Attempted to register null buffer init callback for token {}",
            static_cast<int>(token));
        return;
    }

    m_buffer_init_hooks[token] = callback;

    MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
        "Registered buffer init hook for token {}",
        static_cast<int>(token));
}

void BufferManager::register_buffer_cleanup_hook(ProcessingToken token, const BufferInitCallback& callback)
{
    if (!callback) {
        MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
            "Attempted to register null buffer init callback for token {}",
            static_cast<int>(token));
        return;
    }

    m_buffer_cleanup_hooks[token] = callback;

    MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
        "Registered buffer init hook for token {}",
        static_cast<int>(token));
}

void BufferManager::unregister_buffer_hooks(ProcessingToken token)
{
    auto it = m_buffer_init_hooks.find(token);
    if (it != m_buffer_init_hooks.end()) {
        m_buffer_init_hooks.erase(it);

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Unregistered buffer init hook for token {}",
            static_cast<int>(token));
    }

    auto c_it = m_buffer_cleanup_hooks.find(token);
    if (c_it != m_buffer_cleanup_hooks.end()) {
        m_buffer_cleanup_hooks.erase(c_it);

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Unregistered buffer cleanup hook for token {}",
            static_cast<int>(token));
    }
}

} // namespace MayaFlux::Buffers
