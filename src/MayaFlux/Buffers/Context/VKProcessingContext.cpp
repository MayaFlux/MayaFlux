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

VKProcessingContext::ResourceHandle VKProcessingContext::create_shader_module(const std::string& spirv_path, vk::ShaderStageFlagBits stage)
{
    if (!m_shader_module_creator) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No shader module creator registered");
        return nullptr;
    }
    return m_shader_module_creator(spirv_path, stage);
}

VKProcessingContext::ResourceHandle VKProcessingContext::create_descriptor_manager(uint32_t pool_size)
{
    if (!m_descriptor_manager_creator) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No descriptor manager creator registered");
        return nullptr;
    }
    return m_descriptor_manager_creator(pool_size);
}

vk::DescriptorSetLayout VKProcessingContext::create_descriptor_layout(
    ResourceHandle manager,
    const std::vector<std::pair<uint32_t, vk::DescriptorType>>& bindings)
{
    if (!m_descriptor_layout_creator) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No descriptor layout creator registered");
        return nullptr;
    }
    return m_descriptor_layout_creator(manager, bindings);
}

VKProcessingContext::ResourceHandle VKProcessingContext::create_compute_pipeline(
    ResourceHandle shader,
    const std::vector<vk::DescriptorSetLayout>& layouts,
    uint32_t push_constant_size)
{
    if (!m_compute_pipeline_creator) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No compute pipeline creator registered");
        return nullptr;
    }
    return m_compute_pipeline_creator(shader, layouts, push_constant_size);
}

void VKProcessingContext::cleanup_resource(ResourceHandle resource)
{
    if (m_resource_cleaner && resource) {
        m_resource_cleaner(resource);
    }
}

void VKProcessingContext::set_shader_module_creator(ShaderModuleCreator creator)
{
    m_shader_module_creator = std::move(creator);
}

void VKProcessingContext::set_descriptor_manager_creator(DescriptorManagerCreator creator)
{
    m_descriptor_manager_creator = std::move(creator);
}

void VKProcessingContext::set_descriptor_layout_creator(DescriptorLayoutCreator creator)
{
    m_descriptor_layout_creator = std::move(creator);
}

void VKProcessingContext::set_compute_pipeline_creator(ComputePipelineCreator creator)
{
    m_compute_pipeline_creator = std::move(creator);
}

void VKProcessingContext::set_resource_cleaner(ResourceCleaner cleaner)
{
    m_resource_cleaner = std::move(cleaner);
}

} // namespace MayaFlux::Buffers
