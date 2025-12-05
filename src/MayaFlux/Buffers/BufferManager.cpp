#include "BufferManager.hpp"

#include "BufferProcessingChain.hpp"

#include "Managers/BufferAccessControl.hpp"
#include "Managers/BufferInputControl.hpp"
#include "Managers/BufferProcessingControl.hpp"
#include "Managers/BufferSupplyMixing.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Buffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {

BufferManager::BufferManager(
    uint32_t default_out_channels,
    uint32_t default_in_channels,
    uint32_t default_buffer_size,
    ProcessingToken default_audio_token,
    ProcessingToken default_graphics_token)
    : m_unit_manager(std::make_unique<TokenUnitManager>(default_audio_token, default_graphics_token))
    , m_access_control(std::make_unique<BufferAccessControl>(*m_unit_manager))
    , m_processor_control(std::make_unique<BufferProcessingControl>(*m_unit_manager, *m_access_control))
    , m_input_control(std::make_unique<BufferInputControl>())
    , m_supply_mixing(std::make_unique<BufferSupplyMixing>(*m_unit_manager, *m_access_control))
    , m_global_processing_chain(std::make_shared<BufferProcessingChain>())
{
    validate_num_channels(default_audio_token, default_out_channels, default_buffer_size);

    if (default_in_channels) {
        m_input_control->setup_audio_input_buffers(default_in_channels, default_buffer_size);
    }

    auto& a_unit = m_unit_manager->get_or_create_audio_unit(default_audio_token);
    if (a_unit.channel_count > 0) {
        auto limiter = std::make_shared<FinalLimiterProcessor>();
        m_processor_control->set_audio_final_processor(limiter, default_audio_token);
    }

    auto& g_unit = m_unit_manager->get_or_create_graphics_unit(default_graphics_token);
    auto present_processor = std::make_shared<PresentProcessor>();
    m_processor_control->set_graphics_final_processor(present_processor, default_graphics_token);
}

BufferManager::~BufferManager() = default;

// ============================================================================
// Processing and Token Management
// ============================================================================

void BufferManager::process_token(ProcessingToken token, uint32_t processing_units)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        process_audio_token_default(token, processing_units);
    }

    if (token == ProcessingToken::GRAPHICS_BACKEND) {
        process_graphics_token_default(token, processing_units);
    }
}

void BufferManager::process_all_tokens()
{
    for (const auto& token : m_unit_manager->get_active_audio_tokens()) {
        process_token(token, m_unit_manager->get_audio_buffer_size(token));
    }
}

void BufferManager::process_channel(
    ProcessingToken token,
    uint32_t channel,
    uint32_t /*processing_units*/,
    const std::vector<double>& node_output_data)
{
    if (!m_unit_manager->has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager->get_audio_unit_mutable(token);
    if (channel >= unit.channel_count) {
        return;
    }

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
    std::vector<ProcessingToken> active_tokens;
    for (const auto& token : m_unit_manager->get_active_audio_tokens()) {
        active_tokens.push_back(token);
    }
    for (const auto& token : m_unit_manager->get_active_graphics_tokens()) {
        active_tokens.push_back(token);
    }
    return active_tokens;
}

void BufferManager::register_audio_token_processor(ProcessingToken token, RootAudioProcessingFunction processor)
{
    auto& unit = m_unit_manager->get_or_create_audio_unit(token);
    unit.custom_processor = std::move(processor);
}

ProcessingToken BufferManager::get_default_audio_token() const
{
    return m_unit_manager->get_default_audio_token();
}

// ============================================================================
// Buffer Access (Token-Generic)
// ============================================================================

std::shared_ptr<RootAudioBuffer> BufferManager::get_root_audio_buffer(ProcessingToken token, uint32_t channel)
{
    return m_access_control->get_root_audio_buffer(token, channel);
}

std::shared_ptr<RootGraphicsBuffer> BufferManager::get_root_graphics_buffer(ProcessingToken token)
{
    return m_access_control->get_root_graphics_buffer(token);
}

std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, uint32_t channel)
{
    return m_access_control->get_audio_buffer_data(token, channel);
}

const std::vector<double>& BufferManager::get_buffer_data(ProcessingToken token, uint32_t channel) const
{
    return m_access_control->get_audio_buffer_data(token, channel);
}

uint32_t BufferManager::get_num_channels(ProcessingToken token) const
{
    return m_access_control->get_num_audio_out_channels(token);
}

uint32_t BufferManager::get_buffer_size(ProcessingToken token) const
{
    return m_access_control->get_audio_buffer_size(token);
}

void BufferManager::resize_buffers(ProcessingToken token, uint32_t buffer_size)
{
    if (token != ProcessingToken::AUDIO_BACKEND && token != ProcessingToken::AUDIO_PARALLEL) {
        return;
    }
    m_access_control->resize_audio_buffers(token, buffer_size);
}

void BufferManager::ensure_channels(ProcessingToken token, uint32_t channel_count)
{
    m_access_control->ensure_audio_channels(token, channel_count);
}

std::shared_ptr<BufferProcessingChain> BufferManager::get_processing_chain(ProcessingToken token, uint32_t channel)
{
    return m_access_control->get_audio_processing_chain(token, channel);
}

std::shared_ptr<BufferProcessingChain> BufferManager::get_global_processing_chain()
{
    return m_global_processing_chain;
}

// ============================================================================
// Buffer Management (Token-Generic via Dynamic Dispatch)
// ============================================================================

void BufferManager::add_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    m_access_control->add_buffer(buffer, token, channel);
}

void BufferManager::remove_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    m_access_control->remove_buffer(buffer, token, channel);
}

const std::vector<std::shared_ptr<AudioBuffer>>& BufferManager::get_buffers(ProcessingToken token, uint32_t channel) const
{
    return m_access_control->get_audio_buffers(token, channel);
}

const std::vector<std::shared_ptr<VKBuffer>>& BufferManager::get_graphics_buffers(ProcessingToken token) const
{
    return m_access_control->get_graphics_buffers(token);
}

std::vector<std::shared_ptr<VKBuffer>> BufferManager::get_buffers_by_usage(
    VKBuffer::Usage usage,
    ProcessingToken token) const
{
    return m_access_control->get_graphics_buffers_by_usage(usage, token);
}

// ============================================================================
// Processor Management (Token-Generic)
// ============================================================================

void BufferManager::add_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    m_processor_control->add_processor(processor, buffer, token);
}

void BufferManager::add_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    m_processor_control->add_processor(processor, token, channel);
}

void BufferManager::add_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    m_processor_control->add_processor(processor, token);
}

void BufferManager::remove_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<Buffer>& buffer)
{
    m_processor_control->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_channel(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    m_processor_control->remove_processor_from_token(processor, token, channel);
}

void BufferManager::remove_processor_from_token(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    m_processor_control->remove_processor_from_token(processor, token, 0);
}

void BufferManager::set_final_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    m_processor_control->set_final_processor(processor, token);
}

// ============================================================================
// Quick Processing (Audio-Specific)
// ============================================================================

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(
    AudioProcessingFunction processor,
    const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    return m_processor_control->attach_quick_process(std::move(processor), buffer, token);
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(
    GraphicsProcessingFunction processor,
    const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    return m_processor_control->attach_quick_process(std::move(processor), buffer, token);
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(
    AudioProcessingFunction processor,
    ProcessingToken token,
    uint32_t channel)
{
    return m_processor_control->attach_quick_process(std::move(processor), token, channel);
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(
    AudioProcessingFunction processor,
    ProcessingToken token)
{
    return m_processor_control->attach_quick_process(std::move(processor), token);
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(
    GraphicsProcessingFunction processor,
    ProcessingToken token)
{
    return m_processor_control->attach_quick_process(std::move(processor), token);
}

// ============================================================================
// Node Connection (Audio-Specific)
// ============================================================================

void BufferManager::connect_node_to_channel(
    const std::shared_ptr<Nodes::Node>& node,
    ProcessingToken token,
    uint32_t channel,
    float mix,
    bool clear_before)
{
    m_processor_control->connect_node_to_audio_channel(node, token, channel, mix, clear_before);
}

void BufferManager::connect_node_to_buffer(
    const std::shared_ptr<Nodes::Node>& node,
    const std::shared_ptr<AudioBuffer>& buffer,
    float mix,
    bool clear_before)
{
    m_processor_control->connect_node_to_audio_buffer(node, buffer, mix, clear_before);
}

// ============================================================================
// Data I/O (Audio-Specific)
// ============================================================================

void BufferManager::fill_from_interleaved(
    const double* interleaved_data,
    uint32_t num_frames,
    ProcessingToken token,
    uint32_t num_channels)
{
    m_supply_mixing->fill_audio_from_interleaved(interleaved_data, num_frames, token, num_channels);
}

void BufferManager::fill_interleaved(
    double* interleaved_data,
    uint32_t num_frames,
    ProcessingToken token,
    uint32_t num_channels) const
{
    m_supply_mixing->fill_audio_interleaved(interleaved_data, num_frames, token, num_channels);
}

std::vector<std::shared_ptr<AudioBuffer>> BufferManager::clone_buffer_for_channels(
    const std::shared_ptr<AudioBuffer>& buffer,
    const std::vector<uint32_t>& channels,
    ProcessingToken token)
{
    return m_supply_mixing->clone_audio_buffer_for_channels(buffer, channels, token);
}

// ============================================================================
// Input Handling (Audio-Specific)
// ============================================================================

void BufferManager::process_input(double* input_data, uint32_t num_channels, uint32_t num_frames)
{
    m_input_control->process_audio_input(input_data, num_channels, num_frames);
}

void BufferManager::register_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel)
{
    m_input_control->register_audio_input_listener(buffer, channel);
}

void BufferManager::unregister_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel)
{
    m_input_control->unregister_audio_input_listener(buffer, channel);
}

// ============================================================================
// Buffer Supply/Mixing (Audio-Specific)
// ============================================================================

bool BufferManager::supply_buffer_to(
    const std::shared_ptr<AudioBuffer>& buffer,
    ProcessingToken token,
    uint32_t channel,
    double mix)
{
    return m_supply_mixing->supply_audio_buffer_to(buffer, token, channel, mix);
}

bool BufferManager::remove_supplied_buffer(
    const std::shared_ptr<AudioBuffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    return m_supply_mixing->remove_supplied_audio_buffer(buffer, token, channel);
}

// ============================================================================
// Utility
// ============================================================================

void BufferManager::initialize_buffer_service()
{
    m_access_control->initialize_buffer_service();
}

void BufferManager::terminate_active_buffers()
{
    m_access_control->terminate_active_buffers();
}

// ============================================================================
// Private
// ============================================================================

void BufferManager::process_audio_token_default(ProcessingToken token, uint32_t processing_units)
{
    if (!m_unit_manager->has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager->get_audio_unit_mutable(token);

    if (unit.custom_processor) {
        unit.custom_processor(unit.root_buffers, processing_units);
        return;
    }

    for (uint32_t channel = 0; channel < unit.channel_count; ++channel) {
        process_channel(token, channel, processing_units);
    }
}

void BufferManager::process_graphics_token_default(ProcessingToken token, uint32_t processing_units)
{
    if (!m_unit_manager->has_graphics_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager->get_graphics_unit_mutable(token);

    if (unit.custom_processor) {
        unit.custom_processor(unit.root_buffer, processing_units);
        return;
    }

    auto root_buffer = m_access_control->get_root_graphics_buffer(token);

    // if (!node_output_data.empty()) {
    //     root_buffer->set_node_output(node_output_data);
    // }

    root_buffer->process_default();

    unit.get_chain()->process(root_buffer);

    m_global_processing_chain->process(root_buffer);

    if (auto chain = root_buffer->get_processing_chain()) {
        chain->process_final(root_buffer);
    }
}

} // namespace MayaFlux::Buffers
