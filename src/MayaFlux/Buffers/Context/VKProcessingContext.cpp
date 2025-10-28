#include "VKProcessingContext.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

BufferRegistrationCallback VKProcessingContext::s_buffer_initializer = nullptr;
BufferRegistrationCallback VKProcessingContext::s_buffer_cleaner = nullptr;

void VKProcessingContext::execute_immediate(CommandRecorder recorder)
{
    if (m_execute_immediate) {
        m_execute_immediate(std::move(recorder));
    }
}

void VKProcessingContext::record_deferred(CommandRecorder recorder)
{
    if (m_record_deferred) {
        m_record_deferred(std::move(recorder));
    }
}

void VKProcessingContext::flush_buffer(vk::DeviceMemory memory, size_t offset, size_t size)
{
    if (m_flush) {
        m_flush(memory, offset, size);
    }
}

void VKProcessingContext::invalidate_buffer(vk::DeviceMemory memory, size_t offset, size_t size)
{
    if (m_invalidate) {
        m_invalidate(memory, offset, size);
    }
}

void VKProcessingContext::set_execute_immediate_callback(std::function<void(CommandRecorder)> callback)
{
    m_execute_immediate = std::move(callback);
}

void VKProcessingContext::set_record_deferred_callback(std::function<void(CommandRecorder)> callback)
{
    m_record_deferred = std::move(callback);
}

void VKProcessingContext::set_flush_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback)
{
    m_flush = std::move(callback);
}

void VKProcessingContext::set_invalidate_callback(std::function<void(vk::DeviceMemory, size_t, size_t)> callback)
{
    m_invalidate = std::move(callback);
}

void VKProcessingContext::initialize_buffer(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!s_buffer_initializer) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "No buffer initializer registered in VKProcessingContext");
    }
    s_buffer_initializer(buffer);
}

void VKProcessingContext::cleanup_buffer(const std::shared_ptr<VKBuffer>& buffer)
{
    if (!s_buffer_cleaner) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "No buffer cleaner registered in VKProcessingContext");
    }

    s_buffer_cleaner(buffer);
}

void VKProcessingContext::set_initializer(BufferRegistrationCallback initializer)
{
    s_buffer_initializer = std::move(initializer);
}

void VKProcessingContext::set_cleaner(BufferRegistrationCallback cleaner)
{
    s_buffer_cleaner = std::move(cleaner);
}

} // namespace MayaFlux::Buffers
