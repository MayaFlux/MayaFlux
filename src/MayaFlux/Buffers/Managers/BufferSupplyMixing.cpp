#include "BufferSupplyMixing.hpp"

#include "BufferAccessControl.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Root/MixProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "TokenUnitManager.hpp"

namespace MayaFlux::Buffers {

BufferSupplyMixing::BufferSupplyMixing(
    TokenUnitManager& unit_manager,
    BufferAccessControl& access_control)
    : m_unit_manager(unit_manager)
    , m_access_control(access_control)
{
}

// ============================================================================
// Buffer Supply and Mixing
// ============================================================================

bool BufferSupplyMixing::supply_audio_buffer_to(
    const std::shared_ptr<AudioBuffer>& buffer,
    ProcessingToken token,
    uint32_t channel,
    double mix)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Invalid buffer for supplying");
        return false;
    }

    if (buffer->get_channel_id() == channel) {
        MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Buffer already has the correct channel ID {}", channel);
        return false;
    }

    if (!m_unit_manager.has_audio_unit(token) || channel >= m_unit_manager.get_audio_channel_count(token)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Token/channel combination out of range for supplying (token: {}, channel: {})",
            static_cast<int>(token), channel);
        return false;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    auto root_buffer = unit.get_buffer(channel);
    auto processing_chain = unit.get_chain(channel);

    std::shared_ptr<MixProcessor> mix_processor = processing_chain->get_processor<MixProcessor>(root_buffer);

    if (!mix_processor) {
        mix_processor = std::make_shared<MixProcessor>();
        processing_chain->add_processor(mix_processor, root_buffer);
    }

    return mix_processor->register_source(buffer, mix, false);
}

bool BufferSupplyMixing::remove_supplied_audio_buffer(
    const std::shared_ptr<AudioBuffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Invalid buffer for removal");
        return false;
    }

    if (!m_unit_manager.has_audio_unit(token) || channel >= m_unit_manager.get_audio_channel_count(token)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Token/channel combination out of range for removal (token: {}, channel: {})",
            static_cast<int>(token), channel);
        return false;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    auto root_buffer = unit.get_buffer(channel);
    auto processing_chain = unit.get_chain(channel);

    if (std::shared_ptr<MixProcessor> mix_processor = processing_chain->get_processor<MixProcessor>(root_buffer)) {
        return mix_processor->remove_source(buffer);
    }
    return false;
}

// ============================================================================
// Interleaved Data I/O
// ============================================================================

void BufferSupplyMixing::fill_audio_from_interleaved(
    const double* interleaved_data,
    uint32_t num_frames,
    ProcessingToken token,
    uint32_t num_channels)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    uint32_t channels_to_process = std::min(num_channels, unit.channel_count);

    for (uint32_t channel = 0; channel < channels_to_process; ++channel) {
        auto& buffer_data = unit.get_buffer(channel)->get_data();
        uint32_t frames_to_copy = std::min(num_frames, static_cast<uint32_t>(buffer_data.size()));

        for (uint32_t frame = 0; frame < frames_to_copy; ++frame) {
            buffer_data[frame] = interleaved_data[frame * num_channels + channel];
        }
    }
}

void BufferSupplyMixing::fill_audio_interleaved(
    double* interleaved_data,
    uint32_t num_frames,
    ProcessingToken token,
    uint32_t num_channels) const
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    const auto& unit = m_unit_manager.get_audio_unit(token);
    uint32_t channels_to_process = std::min(num_channels, unit.channel_count);

    for (uint32_t channel = 0; channel < channels_to_process; ++channel) {
        const auto& buffer_data = unit.get_buffer(channel)->get_data();
        uint32_t frames_to_copy = std::min(num_frames, static_cast<uint32_t>(buffer_data.size()));

        for (uint32_t frame = 0; frame < frames_to_copy; ++frame) {
            interleaved_data[frame * num_channels + channel] = buffer_data[frame];
        }
    }
}

// ============================================================================
// Buffer Cloning
// ============================================================================

std::vector<std::shared_ptr<AudioBuffer>> BufferSupplyMixing::clone_audio_buffer_for_channels(
    const std::shared_ptr<AudioBuffer>& buffer,
    const std::vector<uint32_t>& channels,
    ProcessingToken token)
{
    if (channels.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: No channels specified for cloning");
        return {};
    }
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Invalid buffer for cloning");
        return {};
    }

    if (!m_unit_manager.has_audio_unit(token)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Token not found for cloning");
        return {};
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);

    std::vector<std::shared_ptr<AudioBuffer>> cloned_buffers {};

    for (const auto channel : channels) {
        if (channel >= unit.channel_count) {
            MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
                "BufferSupplyMixing: Channel {} out of range for cloning", channel);
            return cloned_buffers;
        }

        auto cloned_buffer = buffer->clone_to(channel);
        m_access_control.add_audio_buffer(cloned_buffer, token, channel);
        cloned_buffers.push_back(cloned_buffer);
    }

    return cloned_buffers;
}

} // namespace MayaFlux::Buffers
