#include "BufferProcessingControl.hpp"

#include "BufferAccessControl.hpp"
#include "TokenUnitManager.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Buffers/Root/MixProcessor.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

class QuickProcess : public BufferProcessor {
public:
    QuickProcess(BufferProcessingFunction function)
        : m_function(std::move(function))
    {
    }

    void processing_function(std::shared_ptr<Buffer> buffer) override
    {
        std::visit([buffer](auto& fn) {
            if constexpr (std::is_same_v<decltype(fn), AudioProcessingFunction&>) {
                if (auto audio_buf = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
                    fn(audio_buf);
                }
            } else {
                if (auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
                    fn(vk_buf);
                }
            }
        },
            m_function);
    }

    void on_attach(std::shared_ptr<Buffer> buffer) override
    {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            if (m_processing_token != ProcessingToken::AUDIO_BACKEND || m_processing_token != ProcessingToken::AUDIO_PARALLEL) {
                m_processing_token = ProcessingToken::AUDIO_BACKEND;
            }
        } else if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            if (m_processing_token != ProcessingToken::GRAPHICS_BACKEND) {
                m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
            }
        } else {
            error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement,
                std::source_location::current(),
                "QuickProcess can only be attached to AudioBuffer or VKBuffer");
        }
    }

    [[nodiscard]] bool is_compatible_with(std::shared_ptr<Buffer> buffer) const override
    {
        return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr || std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
    }

private:
    BufferProcessingFunction m_function;
};

BufferProcessingControl::BufferProcessingControl(
    TokenUnitManager& unit_manager,
    BufferAccessControl& access_control)
    : m_unit_manager(unit_manager)
    , m_access_control(access_control)
{
}

// ============================================================================
// Token-Dispatching Processor Management
// ============================================================================

void BufferProcessingControl::add_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            add_audio_processor(processor, audio_buffer);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        add_graphics_processor(processor, buffer, token);
    }
}

void BufferProcessingControl::add_processor_to_token(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (channel == 0) {
            add_audio_processor_to_token(processor, token);
        } else {
            add_audio_processor_to_channel(processor, token, channel);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        add_graphics_processor(processor, token);
    }
}

void BufferProcessingControl::remove_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            remove_audio_processor(processor, audio_buffer);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        remove_graphics_processor(processor, token);
    }
}

void BufferProcessingControl::remove_processor_from_token(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (channel == 0) {
            remove_audio_processor_from_token(processor, token);
        } else {
            remove_audio_processor_from_channel(processor, token, channel);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        remove_graphics_processor(processor, token);
    }
}

void BufferProcessingControl::set_final_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        set_audio_final_processor(processor, token);
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        set_graphics_final_processor(processor, token);
    }
}

// ============================================================================
// Processor Management - Audio
// ============================================================================

void BufferProcessingControl::add_audio_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<AudioBuffer>& buffer)
{
    uint32_t channel_id = buffer->get_channel_id();

    for (const auto& token : m_unit_manager.get_active_audio_tokens()) {
        const auto& unit = m_unit_manager.get_audio_unit(token);
        if (channel_id < unit.channel_count) {
            auto processing_chain = unit.get_chain(channel_id);
            processing_chain->add_processor(processor, buffer);
            buffer->set_processing_chain(processing_chain);
            return;
        }
    }

    // Fallback: use global processing chain if no token matches
    // This requires BufferManager to provide access to global chain
    // For now, we log a warning
    MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
        "Could not find matching token for audio buffer with channel ID {}", channel_id);
}

void BufferProcessingControl::add_audio_processor_to_channel(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    auto chain = m_access_control.get_audio_processing_chain(token, channel);
    auto root_buffer = m_access_control.get_root_audio_buffer(token, channel);
    chain->add_processor(processor, root_buffer);
}

void BufferProcessingControl::add_audio_processor_to_token(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    auto& unit = m_unit_manager.get_or_create_audio_unit(token);
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        auto chain = unit.get_chain(i);
        auto root_buffer = unit.get_buffer(i);
        chain->add_processor(processor, root_buffer);
    }
}

void BufferProcessingControl::remove_audio_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<AudioBuffer>& buffer)
{
    uint32_t channel_id = buffer->get_channel_id();

    for (const auto& token : m_unit_manager.get_active_audio_tokens()) {
        const auto& unit = m_unit_manager.get_audio_unit(token);
        if (channel_id < unit.channel_count) {
            auto processing_chain = unit.get_chain(channel_id);
            processing_chain->remove_processor(processor, buffer);
            if (auto buf_chain = buffer->get_processing_chain()) {
                buf_chain->remove_processor(processor, buffer);
            }
            return;
        }
    }
}

void BufferProcessingControl::remove_audio_processor_from_channel(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token,
    uint32_t channel)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    if (channel >= unit.channel_count) {
        return;
    }

    auto chain = unit.get_chain(channel);
    auto root_buffer = unit.get_buffer(channel);
    chain->remove_processor(processor, root_buffer);
}

void BufferProcessingControl::remove_audio_processor_from_token(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        auto chain = unit.get_chain(i);
        auto root_buffer = unit.get_buffer(i);
        chain->remove_processor(processor, root_buffer);
    }
}

void BufferProcessingControl::set_audio_final_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    if (!m_unit_manager.has_audio_unit(token)) {
        return;
    }

    auto& unit = m_unit_manager.get_audio_unit_mutable(token);
    for (uint32_t i = 0; i < unit.channel_count; ++i) {
        auto chain = unit.get_chain(i);
        auto root_buffer = unit.get_buffer(i);
        chain->add_final_processor(processor, root_buffer);
    }
}

// ============================================================================
// Quick Processing - Audio
// ============================================================================

std::shared_ptr<BufferProcessor> BufferProcessingControl::attach_quick_process(
    BufferProcessingFunction processor,
    const std::shared_ptr<Buffer>& buffer, ProcessingToken token)
{
    auto quick_process = std::make_shared<QuickProcess>(std::move(processor));
    add_processor(quick_process, buffer, token);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferProcessingControl::attach_quick_process(
    BufferProcessingFunction processor,
    ProcessingToken token,
    uint32_t channel)
{
    auto quick_process = std::make_shared<QuickProcess>(std::move(processor));
    add_audio_processor_to_channel(quick_process, token, channel);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferProcessingControl::attach_quick_process(
    BufferProcessingFunction processor,
    ProcessingToken token)
{
    auto quick_process = std::make_shared<QuickProcess>(std::move(processor));
    add_processor_to_token(quick_process, token);
    return quick_process;
}

// ============================================================================
// Node Connection - Audio
// ============================================================================

void BufferProcessingControl::connect_node_to_audio_channel(
    const std::shared_ptr<Nodes::Node>& node,
    ProcessingToken token,
    uint32_t channel,
    float mix,
    bool clear_before)
{
    m_access_control.ensure_audio_channels(token, channel + 1);

    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);
    processor->set_processing_token(token);

    add_audio_processor_to_channel(processor, token, channel);
}

void BufferProcessingControl::connect_node_to_audio_buffer(
    const std::shared_ptr<Nodes::Node>& node,
    const std::shared_ptr<AudioBuffer>& buffer,
    float mix,
    bool clear_before)
{
    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);
    add_audio_processor(processor, buffer);
}

// ============================================================================
// Processor Management - Graphics
// ============================================================================

void BufferProcessingControl::add_graphics_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    auto chain = m_access_control.get_graphics_processing_chain(token);
    auto root_buffer = m_access_control.get_root_graphics_buffer(token);
    chain->add_processor(processor, root_buffer);
}

void BufferProcessingControl::add_graphics_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token)
{
    auto chain = m_access_control.get_graphics_processing_chain(token);
    chain->add_processor(processor, buffer);
    buffer->set_processing_chain(chain);
}

void BufferProcessingControl::set_graphics_final_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    auto& unit = m_unit_manager.get_graphics_unit_mutable(token);
    auto chain = unit.get_chain();
    auto root_buffer = unit.get_buffer();
    chain->add_final_processor(processor, root_buffer);
}

void BufferProcessingControl::remove_graphics_processor(
    const std::shared_ptr<BufferProcessor>& processor,
    ProcessingToken token)
{
    if (!m_unit_manager.has_graphics_unit(token)) {
        MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
            "Token {} not found when removing graphics processor",
            static_cast<int>(token));
        return;
    }

    auto& unit = m_unit_manager.get_graphics_unit_mutable(token);
    auto chain = unit.get_chain();
    auto root_buffer = unit.get_buffer();
    chain->remove_processor(processor, root_buffer);
}

} // namespace MayaFlux::Buffers
