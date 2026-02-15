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

void BufferSupplyMixing::route_buffer_to_channel(
    const std::shared_ptr<AudioBuffer>& buffer,
    uint32_t target_channel,
    uint32_t fade_cycles,
    ProcessingToken token)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Invalid buffer for routing");
        return;
    }

    uint32_t current_channel = buffer->get_channel_id();

    if (current_channel == target_channel) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Buffer already on target channel {}", target_channel);
        return;
    }

    if (!m_unit_manager.has_audio_unit(token)) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Invalid token for routing");
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    if (target_channel >= unit.channel_count) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "BufferSupplyMixing: Target channel {} out of range", target_channel);
        return;
    }

    uint32_t fade_blocks = std::max(1U, fade_cycles);

    BufferRoutingState state;
    state.from_channel = current_channel;
    state.to_channel = target_channel;
    state.fade_cycles = fade_blocks;
    state.from_amount = 1.0;
    state.to_amount = 0.0;
    state.cycles_elapsed = 0;
    state.phase = BufferRoutingState::ACTIVE;

    buffer->get_routing_state() = state;

    supply_audio_buffer_to(buffer, token, target_channel, 0.0);
}

void BufferSupplyMixing::update_routing_states_for_cycle(ProcessingToken token)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);

    for (uint32_t channel = 0; channel < unit.channel_count; ++channel) {
        auto root_buffer = unit.get_buffer(channel);

        for (auto& child : root_buffer->get_child_buffers()) {
            if (!child->needs_routing()) {
                continue;
            }

            auto& state = child->get_routing_state();

            if (state.phase != BufferRoutingState::ACTIVE) {
                continue;
            }

            update_routing_state(state);

            auto root_target = unit.get_buffer(state.to_channel);
            auto chain_target = unit.get_chain(state.to_channel);

            if (auto mix_proc = chain_target->get_processor<MixProcessor>(root_target)) {
                mix_proc->update_source_mix(child, state.to_amount);
            }
        }
    }
}

void BufferSupplyMixing::cleanup_completed_routing(ProcessingToken token)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);

    std::vector<std::tuple<std::shared_ptr<AudioBuffer>, uint32_t, uint32_t>> buffers_to_move;

    for (uint32_t channel = 0; channel < unit.channel_count; ++channel) {
        auto root_buffer = unit.get_buffer(channel);

        for (auto& child : root_buffer->get_child_buffers()) {
            if (!child->needs_routing()) {
                continue;
            }

            auto& state = child->get_routing_state();

            if (state.phase == BufferRoutingState::COMPLETED) {
                buffers_to_move.emplace_back(child, state.from_channel, state.to_channel);
            }
        }
    }

    for (auto& [buffer, from_ch, to_ch] : buffers_to_move) {
        auto root_from = unit.get_buffer(from_ch);
        auto root_to = unit.get_buffer(to_ch);

        root_from->remove_child_buffer(buffer);

        buffer->set_channel_id(to_ch);
        root_to->add_child_buffer(buffer);

        remove_supplied_audio_buffer(buffer, token, to_ch);

        buffer->get_routing_state() = BufferRoutingState {};
    }
}

void BufferSupplyMixing::update_routing_state(BufferRoutingState& state)
{
    state.cycles_elapsed++;

    double progress = 0.0;
    if (state.fade_cycles > 0) {
        progress = std::min(1.0, static_cast<double>(state.cycles_elapsed) / state.fade_cycles);
    } else {
        state.from_amount = 0.0;
        state.to_amount = 1.0;
        state.phase = BufferRoutingState::COMPLETED;
        return;
    }

    state.from_amount = 1.0 - progress;
    state.to_amount = progress;

    if (state.cycles_elapsed >= state.fade_cycles) {
        state.phase = BufferRoutingState::COMPLETED;
    }
}

} // namespace MayaFlux::Buffers
