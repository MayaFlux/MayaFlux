#include "BackendPipelineManager.hpp"

#include "VKComputePipeline.hpp"
#include "VKContext.hpp"
#include "VKDescriptorManager.hpp"
#include "VKGraphicsPipeline.hpp"
#include "VKShaderModule.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/ComputeService.hpp"

namespace MayaFlux::Core {

BackendPipelineManager::BackendPipelineManager(VKContext& context)
    : m_context(context)
{
}

void BackendPipelineManager::setup_backend_service(const std::shared_ptr<Registry::Service::ComputeService>& compute_service)
{
    compute_service->create_shader_module = [this](const std::string& path, uint32_t stage) -> std::shared_ptr<void> {
        return std::static_pointer_cast<void>(this->create_shader_module(path, stage));
    };
    compute_service->create_descriptor_manager = [this](uint32_t pool_size) -> std::shared_ptr<void> {
        return std::static_pointer_cast<void>(this->create_descriptor_manager(pool_size));
    };
    compute_service->create_descriptor_layout = [this](const std::shared_ptr<void>& mgr,
                                                    const std::vector<std::pair<uint32_t, uint32_t>>& bindings) -> void* {
        auto manager = std::static_pointer_cast<VKDescriptorManager>(mgr);
        vk::DescriptorSetLayout layout = this->create_descriptor_layout(manager, bindings);
        return reinterpret_cast<void*>(static_cast<VkDescriptorSetLayout>(layout));
    };
    compute_service->create_compute_pipeline = [this](const std::shared_ptr<void>& shdr,
                                                   const std::vector<void*>& layouts,
                                                   uint32_t push_size) -> std::shared_ptr<void> {
        auto shader = std::static_pointer_cast<VKShaderModule>(shdr);
        std::vector<vk::DescriptorSetLayout> vk_layouts;

        vk_layouts.reserve(layouts.size());
        for (auto* ptr : layouts) {
            vk_layouts.emplace_back(reinterpret_cast<VkDescriptorSetLayout>(ptr));
        }

        return std::static_pointer_cast<void>(this->create_compute_pipeline(shader, vk_layouts, push_size));
    };
    compute_service->cleanup_resource = [this](const std::shared_ptr<void>& res) {
        this->cleanup_compute_resource(res.get());
    };
}

std::shared_ptr<VKShaderModule> BackendPipelineManager::create_shader_module(const std::string& spirv_path, uint32_t stage)
{
    auto shader = std::make_shared<VKShaderModule>();
    shader->create_from_spirv_file(m_context.get_device(), spirv_path, vk::ShaderStageFlagBits(stage));

    track_shader(shader);
    return shader;
}

std::shared_ptr<VKDescriptorManager> BackendPipelineManager::create_descriptor_manager(uint32_t pool_size)
{
    auto manager = std::make_shared<VKDescriptorManager>();
    manager->initialize(m_context.get_device(), pool_size);

    track_descriptor_manager(manager);
    return manager;
}

vk::DescriptorSetLayout BackendPipelineManager::create_descriptor_layout(
    const std::shared_ptr<VKDescriptorManager>& manager,
    const std::vector<std::pair<uint32_t, uint32_t>>& bindings)
{
    DescriptorSetLayoutConfig vk_bindings {};
    for (const auto& [binding, type] : bindings) {
        vk_bindings.add_binding(binding, vk::DescriptorType(type), vk::ShaderStageFlagBits::eCompute);
    }

    return manager->create_layout(m_context.get_device(), vk_bindings);
}

std::shared_ptr<VKComputePipeline> BackendPipelineManager::create_compute_pipeline(
    const std::shared_ptr<VKShaderModule>& shader,
    const std::vector<vk::DescriptorSetLayout>& layouts,
    uint32_t push_constant_size)
{
    auto pipeline = std::make_shared<VKComputePipeline>();
    ComputePipelineConfig config;
    config.shader = shader;
    config.set_layouts = layouts;
    if (push_constant_size > 0) {
        config.add_push_constant(vk::ShaderStageFlagBits::eCompute, push_constant_size);
    }

    pipeline->create(m_context.get_device(), config);

    track_compute_pipeline(pipeline);
    return pipeline;
}

std::shared_ptr<VKGraphicsPipeline> BackendPipelineManager::create_graphics_pipeline(
    const GraphicsPipelineConfig& config)
{
    auto pipeline = std::make_shared<VKGraphicsPipeline>();

    if (!pipeline->create(m_context.get_device(), config)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create graphics pipeline");
        return nullptr;
    }

    track_graphics_pipeline(pipeline);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created graphics pipeline");

    return pipeline;
}

void BackendPipelineManager::cleanup_compute_resource(void* /*resource*/)
{
    // Resource will be cleaned up via shared_ptr destruction
    // If you need explicit cleanup, add it here
    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Cleanup compute resource requested (handled by shared_ptr)");
}

void BackendPipelineManager::cleanup()
{
    m_managed_graphics_pipelines.clear();
    m_managed_compute_pipelines.clear();
    m_managed_descriptor_managers.clear();
    m_managed_shaders.clear();

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Pipeline manager cleanup completed");
}

void BackendPipelineManager::track_shader(const std::shared_ptr<VKShaderModule>& shader)
{
    m_managed_shaders.push_back(shader);
}

void BackendPipelineManager::track_descriptor_manager(const std::shared_ptr<VKDescriptorManager>& manager)
{
    m_managed_descriptor_managers.push_back(manager);
}

void BackendPipelineManager::track_compute_pipeline(const std::shared_ptr<VKComputePipeline>& pipeline)
{
    m_managed_compute_pipelines.push_back(pipeline);
}

void BackendPipelineManager::track_graphics_pipeline(const std::shared_ptr<VKGraphicsPipeline>& pipeline)
{
    m_managed_graphics_pipelines.push_back(pipeline);
}
}
