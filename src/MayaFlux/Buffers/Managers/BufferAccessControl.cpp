#include "BufferAccessControl.hpp"

#include "TokenUnitManager.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

BufferAccessControl::BufferAccessControl(TokenUnitManager& unit_manager)
    : m_unit_manager(unit_manager)
{
}

// ============================================================================
// Audio Buffer Data Access
// ============================================================================

std::vector<double>& BufferAccessControl::get_audio_buffer_data(
    ProcessingToken token,
    uint32_t channel)
{
    return get_root_audio_buffer(token, channel)->get_data();
}

const std::vector<double>& BufferAccessControl::get_audio_buffer_data(
    ProcessingToken token,
    uint32_t channel) const
{
    return get_root_audio_buffer(token, channel)->get_data();
}

// ============================================================================
// Audio Channel and Sizing Operations
// ============================================================================

uint32_t BufferAccessControl::get_num_audio_out_channels(ProcessingToken token) const
{
    return m_unit_manager.get_audio_channel_count(token);
}

uint32_t BufferAccessControl::get_audio_buffer_size(ProcessingToken token) const
{
    return m_unit_manager.get_audio_buffer_size(token);
}

void BufferAccessControl::resize_audio_buffers(ProcessingToken token, uint32_t buffer_size)
{
    m_unit_manager.resize_audio_buffers(token, buffer_size);
}

void BufferAccessControl::ensure_audio_channels(ProcessingToken token, uint32_t channel_count)
{
    m_unit_manager.ensure_audio_channels(token, channel_count);
}

// ============================================================================
// Root Buffer Access (Audio)
// ============================================================================

std::shared_ptr<RootAudioBuffer> BufferAccessControl::get_root_audio_buffer(
    ProcessingToken token,
    uint32_t channel)
{
    if (token != ProcessingToken::AUDIO_BACKEND && token != ProcessingToken::AUDIO_PARALLEL) {
        error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement,
            std::source_location::current(),
            "Invalid token for audio buffer access: {}", static_cast<int>(token));
    }
    auto& unit = m_unit_manager.get_or_create_audio_unit(token);
    return unit.get_buffer(channel);
}

std::shared_ptr<const RootAudioBuffer> BufferAccessControl::get_root_audio_buffer(
    ProcessingToken token,
    uint32_t channel) const
{
    const auto& unit = m_unit_manager.get_audio_unit(token);
    return unit.get_buffer(channel);
}

// ============================================================================
// Root Buffer Access (Graphics)
// ============================================================================

std::shared_ptr<RootGraphicsBuffer> BufferAccessControl::get_root_graphics_buffer(
    ProcessingToken token)
{
    auto& unit = m_unit_manager.get_or_create_graphics_unit(token);
    return unit.get_buffer();
}

std::shared_ptr<const RootGraphicsBuffer> BufferAccessControl::get_root_graphics_buffer(
    ProcessingToken token) const
{
    const auto& unit = m_unit_manager.get_graphics_unit(token);
    return unit.get_buffer();
}

// ============================================================================
// Token-Dispatching Buffer Management
// ============================================================================

void BufferAccessControl::add_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            add_audio_buffer(audio_buffer, token, channel);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            add_graphics_buffer(vk_buffer, token);
        }
    }
}

void BufferAccessControl::remove_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    if (token == ProcessingToken::AUDIO_BACKEND || token == ProcessingToken::AUDIO_PARALLEL) {
        if (auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer)) {
            remove_audio_buffer(audio_buffer, token, channel);
        }
    } else if (token == ProcessingToken::GRAPHICS_BACKEND) {
        if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
            remove_graphics_buffer(vk_buffer, token);
        }
    }
}

// ============================================================================
// Audio Buffer Hierarchy Management
// ============================================================================

void BufferAccessControl::add_audio_buffer(
    const std::shared_ptr<AudioBuffer>& buffer,
    ProcessingToken token,
    uint32_t channel)
{
    ensure_audio_channels(token, channel + 1);

    auto& unit = m_unit_manager.get_or_create_audio_unit(token);
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
            MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
                "Resizing buffer to match unit size: {} samples", unit.buffer_size);

            std::lock_guard<std::mutex> lock(m_unit_manager.get_mutex());
            buffer->resize(unit.buffer_size);
        }
    }

    unit.get_buffer(channel)->add_child_buffer(buffer);
}

void BufferAccessControl::remove_audio_buffer(
    const std::shared_ptr<AudioBuffer>& buffer,
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

    unit.get_buffer(channel)->remove_child_buffer(buffer);
}

const std::vector<std::shared_ptr<AudioBuffer>>& BufferAccessControl::get_audio_buffers(
    ProcessingToken token,
    uint32_t channel) const
{
    const auto& unit = m_unit_manager.get_audio_unit(token);
    if (channel >= unit.channel_count) {
        throw std::out_of_range("Audio channel out of range");
    }
    return unit.get_buffer(channel)->get_child_buffers();
}

// ============================================================================
// Graphics Buffer Hierarchy Management
// ============================================================================

void BufferAccessControl::add_graphics_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token)
{
    if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {

        auto& unit = m_unit_manager.get_or_create_graphics_unit(token);
        auto processing_chain = unit.get_chain();

        if (auto buf_chain = buffer->get_processing_chain()) {
            if (buf_chain != processing_chain) {
                processing_chain->merge_chain(buf_chain);
            }
        } else {
            buffer->set_processing_chain(processing_chain);
        }

        try {
            if (!vk_buffer->is_initialized()) {
                if (!m_buffer_service) {
                    initialize_buffer_service();
                }
                m_buffer_service->initialize_buffer(vk_buffer);
            }
        } catch (const std::exception& e) {
            error_rethrow(Journal::Component::Core, Journal::Context::BufferManagement,
                std::source_location::current(),
                "Failed to initialize graphics buffer for token {}: {}",
                static_cast<int>(token), e.what());
        }

        unit.get_buffer()->add_child_buffer(vk_buffer);

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Added graphics buffer to token {} (total: {})",
            static_cast<int>(token),
            unit.get_buffer()->get_buffer_count());

    } else {
        error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement,
            std::source_location::current(),
            "Unsupported graphics buffer type for token {}",
            static_cast<int>(token));
    }
}

void BufferAccessControl::remove_graphics_buffer(
    const std::shared_ptr<Buffer>& buffer,
    ProcessingToken token)
{
    if (!buffer) {
        MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
            "Attempted to remove null graphics buffer from token {}",
            static_cast<int>(token));
        return;
    }

    if (auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer)) {
        if (!m_unit_manager.has_graphics_unit(token)) {
            MF_WARN(Journal::Component::Core, Journal::Context::BufferManagement,
                "Token {} not found when removing graphics buffer",
                static_cast<int>(token));
            return;
        }

        auto& unit = m_unit_manager.get_graphics_unit_mutable(token);
        unit.get_buffer()->remove_child_buffer(vk_buffer);

        try {
            auto buffer_service = Registry::BackendRegistry::instance()
                                      .get_service<Registry::Service::BufferService>();
            if (vk_buffer->is_initialized()) {
                buffer_service->destroy_buffer(vk_buffer);
            }
        } catch (const std::exception& e) {
            error_rethrow(Journal::Component::Core, Journal::Context::BufferManagement,
                std::source_location::current(),
                "Failed to cleanup graphics buffer for token {}: {}",
                static_cast<int>(token), e.what());
        }

        MF_INFO(Journal::Component::Core, Journal::Context::BufferManagement,
            "Removed graphics buffer from token {} (remaining: {})",
            static_cast<int>(token),
            unit.get_buffer()->get_buffer_count());
    } else {
        error<std::invalid_argument>(Journal::Component::Core, Journal::Context::BufferManagement,
            std::source_location::current(),
            "Unsupported graphics buffer type for token {}",
            static_cast<int>(token));
    }
}

const std::vector<std::shared_ptr<VKBuffer>>& BufferAccessControl::get_graphics_buffers(
    ProcessingToken token) const
{
    const auto& unit = m_unit_manager.get_graphics_unit(token);
    return unit.get_buffer()->get_child_buffers();
}

std::vector<std::shared_ptr<VKBuffer>> BufferAccessControl::get_graphics_buffers_by_usage(
    VKBuffer::Usage usage,
    ProcessingToken token) const
{
    if (!m_unit_manager.has_graphics_unit(token)) {
        return {};
    }

    const auto& unit = m_unit_manager.get_graphics_unit(token);
    return unit.get_buffer()->get_buffers_by_usage(usage);
}

// ============================================================================
// Processing Chain Access
// ============================================================================

std::shared_ptr<BufferProcessingChain> BufferAccessControl::get_audio_processing_chain(
    ProcessingToken token,
    uint32_t channel)
{
    ensure_audio_channels(token, channel + 1);
    auto& unit = m_unit_manager.get_or_create_audio_unit(token);
    return unit.get_chain(channel);
}

std::shared_ptr<BufferProcessingChain> BufferAccessControl::get_graphics_processing_chain(
    ProcessingToken token)
{
    auto& unit = m_unit_manager.get_or_create_graphics_unit(token);
    return unit.get_chain();
}

void BufferAccessControl::initialize_buffer_service()
{
    m_buffer_service = Registry::BackendRegistry::instance()
                           .get_service<Registry::Service::BufferService>();
}

void BufferAccessControl::terminate_active_buffers()
{
    for (const auto& token : m_unit_manager.get_active_audio_tokens()) {
        auto& unit = m_unit_manager.get_audio_unit_mutable(token);
        for (uint32_t channel = 0; channel < unit.channel_count; ++channel) {
            auto root_buffer = unit.get_buffer(channel);
            root_buffer->clear();
            for (auto& child : root_buffer->get_child_buffers()) {
                child->clear();
            }
        }
    }

    for (const auto& token : m_unit_manager.get_active_graphics_tokens()) {
        auto& unit = m_unit_manager.get_graphics_unit_mutable(token);
        auto root_buffer = unit.get_buffer();
        root_buffer->clear();
        for (auto& child : root_buffer->get_child_buffers()) {
            remove_graphics_buffer(child, token);
            child->clear();
        }
    }
}

} // namespace MayaFlux::Buffers
