#include "ShaderFoundry.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/BackendResoureManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKComputePipeline.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDescriptorManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKShaderModule.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool ShaderFoundry::initialize(
    const std::shared_ptr<Core::VulkanBackend>& backend,
    const ShaderCompilerConfig& config)
{
    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Cannot initialize ShaderFoundry with null backend");
        return false;
    }

    if (m_backend) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry already initialized");
        return true;
    }

    m_backend = backend;
    m_config = config;

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "ShaderFoundry initialized");
    return true;
}

void ShaderFoundry::shutdown()
{
    if (!m_backend) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Shutting down ShaderFoundry...");

    m_shader_cache.clear();
    m_backend = nullptr;

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "ShaderFoundry shutdown complete");
}

//==============================================================================
// Shader Compilation - Primary API
//==============================================================================

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::compile_from_file(
    const std::string& filepath,
    std::optional<ShaderStage> stage,
    const std::string& entry_point)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry not initialized");
        return nullptr;
    }

    auto it = m_shader_cache.find(filepath);
    if (it != m_shader_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader: {}", filepath);
        return it->second;
    }

    std::optional<vk::ShaderStageFlagBits> vk_stage;
    if (stage.has_value()) {
        vk_stage = to_vulkan_stage(*stage);
    }

    auto shader = create_shader_module();

    if (filepath.ends_with(".spv")) {
        if (!shader->create_from_spirv_file(
                get_device(), filepath,
                vk_stage.value_or(vk::ShaderStageFlagBits::eCompute),
                entry_point, m_config.enable_reflection)) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
                "Failed to compile SPIR-V shader: {}", filepath);
            return nullptr;
        }
    } else {
        if (!shader->create_from_glsl_file(
                get_device(), filepath, vk_stage, entry_point,
                m_config.enable_reflection, m_config.include_directories,
                m_config.defines)) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
                "Failed to compile GLSL shader: {}", filepath);
            return nullptr;
        }
    }

    m_shader_cache[filepath] = shader;
    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Compiled shader: {} ({})", filepath, vk::to_string(shader->get_stage()));
    return shader;
}

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::compile_from_source(
    const std::string& source,
    ShaderStage stage,
    const std::string& entry_point)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry not initialized");
        return nullptr;
    }

    auto shader = create_shader_module();
    auto vk_stage = to_vulkan_stage(stage);

    if (!shader->create_from_glsl(
            get_device(), source, vk_stage, entry_point,
            m_config.enable_reflection, m_config.include_directories,
            m_config.defines)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Failed to compile GLSL source");
        return nullptr;
    }

    MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Compiled shader from source ({})", vk::to_string(vk_stage));
    return shader;
}

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::compile_from_source_cached(
    const std::string& source,
    ShaderStage stage,
    const std::string& cache_key,
    const std::string& entry_point)
{
    auto it = m_shader_cache.find(cache_key);
    if (it != m_shader_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader: {}", cache_key);
        return it->second;
    }

    auto shader = compile_from_source(source, stage, entry_point);
    if (!shader) {
        return nullptr;
    }

    m_shader_cache[cache_key] = shader;
    return shader;
}

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::compile_from_spirv(
    const std::string& spirv_path,
    ShaderStage stage,
    const std::string& entry_point)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry not initialized");
        return nullptr;
    }

    auto it = m_shader_cache.find(spirv_path);
    if (it != m_shader_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached SPIR-V shader: {}", spirv_path);
        return it->second;
    }

    auto shader = create_shader_module();
    auto vk_stage = to_vulkan_stage(stage);

    if (!shader->create_from_spirv_file(
            get_device(), spirv_path, vk_stage, entry_point,
            m_config.enable_reflection)) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Failed to load SPIR-V shader: {}", spirv_path);
        return nullptr;
    }

    m_shader_cache[spirv_path] = shader;
    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Loaded SPIR-V shader: {}", spirv_path);
    return shader;
}

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::compile(const ShaderSource& shader_source)
{
    switch (shader_source.type) {
    case ShaderSource::SourceType::GLSL_FILE:
        return compile_from_file(shader_source.content, shader_source.stage, shader_source.entry_point);

    case ShaderSource::SourceType::GLSL_STRING:
        return compile_from_source(shader_source.content, shader_source.stage, shader_source.entry_point);

    case ShaderSource::SourceType::SPIRV_FILE:
        return compile_from_spirv(shader_source.content, shader_source.stage, shader_source.entry_point);

    default:
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Unknown shader source type");
        return nullptr;
    }
}

ShaderID ShaderFoundry::load_shader(
    const std::string& filepath,
    std::optional<ShaderStage> stage,
    const std::string& entry_point)
{
    auto cache_it = m_shader_filepath_cache.find(filepath);
    if (cache_it != m_shader_filepath_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader ID for: {}", filepath);
        return cache_it->second;
    }

    auto shader_module = compile_from_file(filepath, stage, entry_point);
    if (!shader_module) {
        return INVALID_SHADER;
    }

    ShaderID id = m_next_shader_id++;

    ShaderState& state = m_shaders[id];
    state.module = shader_module;
    state.filepath = filepath;
    state.stage = stage.value_or(
        detect_stage_from_extension(filepath).value_or(ShaderStage::COMPUTE));
    state.entry_point = entry_point;

    m_shader_filepath_cache[filepath] = id;

    return id;
}

ShaderID ShaderFoundry::load_shader_from_source(
    const std::string& source,
    ShaderStage stage,
    const std::string& entry_point)
{
    auto shader_module = compile_from_source(source, stage, entry_point);
    if (!shader_module) {
        return INVALID_SHADER;
    }

    ShaderID id = m_next_shader_id++;

    ShaderState& state = m_shaders[id];
    state.module = shader_module;
    state.stage = stage;
    state.entry_point = entry_point;
    // No filepath for source-based shaders

    return id;
}

ShaderID ShaderFoundry::load_shader_from_source_cached(
    const std::string& source,
    ShaderStage stage,
    const std::string& cache_key,
    const std::string& entry_point)
{
    auto cache_it = m_shader_filepath_cache.find(cache_key);
    if (cache_it != m_shader_filepath_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader ID for: {}", cache_key);
        return cache_it->second;
    }

    auto shader_module = compile_from_source_cached(source, stage, cache_key, entry_point);
    if (!shader_module) {
        return INVALID_SHADER;
    }

    ShaderID id = m_next_shader_id++;

    ShaderState& state = m_shaders[id];
    state.module = shader_module;
    state.stage = stage;
    state.entry_point = entry_point;

    m_shader_filepath_cache[cache_key] = id;

    return id;
}

ShaderID ShaderFoundry::load_shader_from_spirv(
    const std::string& spirv_path,
    ShaderStage stage,
    const std::string& entry_point)
{
    auto cache_it = m_shader_filepath_cache.find(spirv_path);
    if (cache_it != m_shader_filepath_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader ID for: {}", spirv_path);
        return cache_it->second;
    }

    auto shader_module = compile_from_spirv(spirv_path, stage, entry_point);

    if (!shader_module) {
        return INVALID_SHADER;
    }

    ShaderID id = m_next_shader_id++;

    ShaderState& state = m_shaders[id];
    state.module = shader_module;
    state.filepath = spirv_path;
    state.stage = stage;
    state.entry_point = entry_point;

    m_shader_filepath_cache[spirv_path] = id;

    return id;
}

ShaderID ShaderFoundry::reload_shader(const std::string& filepath)
{
    // Invalidate cache
    invalidate_cache(filepath);

    // Remove from filepath cache
    auto cache_it = m_shader_filepath_cache.find(filepath);
    if (cache_it != m_shader_filepath_cache.end()) {
        // Destroy old shader state
        destroy_shader(cache_it->second);
        m_shader_filepath_cache.erase(cache_it);
    }

    // Reload
    return load_shader(filepath);
}

void ShaderFoundry::destroy_shader(ShaderID shader_id)
{
    auto it = m_shaders.find(shader_id);
    if (it != m_shaders.end()) {
        // Remove from filepath cache
        if (!it->second.filepath.empty()) {
            m_shader_filepath_cache.erase(it->second.filepath);
        }

        // Remove from shader cache (VKShaderModule cache)
        if (!it->second.filepath.empty()) {
            m_shader_cache.erase(it->second.filepath);
        }

        m_shaders.erase(it);
    }
}

//==============================================================================
// Hot-Reload Support
//==============================================================================

void ShaderFoundry::invalidate_cache(const std::string& cache_key)
{
    auto it = m_shader_cache.find(cache_key);
    if (it != m_shader_cache.end()) {
        m_shader_cache.erase(it);
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Invalidated shader cache: {}", cache_key);
    }
}

void ShaderFoundry::clear_cache()
{
    m_shader_cache.clear();
    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Cleared shader cache");
}

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::hot_reload(const std::string& filepath)
{
    invalidate_cache(filepath);
    return compile_from_file(filepath);
}

//==============================================================================
// Configuration
//==============================================================================

void ShaderFoundry::set_config(const ShaderCompilerConfig& config)
{
    m_config = config;
    MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Updated shader compiler configuration");
}

void ShaderFoundry::add_include_directory(const std::string& directory)
{
    m_config.include_directories.push_back(directory);
}

void ShaderFoundry::add_define(const std::string& name, const std::string& value)
{
    m_config.defines[name] = value;
}

//==============================================================================
// Introspection
//==============================================================================

bool ShaderFoundry::is_cached(const std::string& cache_key) const
{
    return m_shader_cache.find(cache_key) != m_shader_cache.end();
}

std::vector<std::string> ShaderFoundry::get_cached_keys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_shader_cache.size());
    for (const auto& [key, _] : m_shader_cache) {
        keys.push_back(key);
    }
    return keys;
}

//==============================================================================
// Pipeline Management
//==============================================================================

PipelineID ShaderFoundry::create_compute_pipeline(
    std::shared_ptr<Core::VKShaderModule> shader,
    const std::vector<std::vector<DescriptorBindingConfig>>& descriptor_sets,
    size_t push_constant_size)
{
    PipelineID id = m_next_pipeline_id++;

    PipelineState& state = m_pipelines[id];

    state.descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
    state.descriptor_manager->initialize(get_device(), 16);

    for (const auto& set_bindings : descriptor_sets) {
        Core::DescriptorSetLayoutConfig layout_config;

        for (const auto& binding : set_bindings) {
            vk::DescriptorType vk_type = binding.type;
            layout_config.add_binding(
                binding.binding,
                vk_type,
                vk::ShaderStageFlagBits::eCompute,
                1);
        }

        state.layouts.push_back(
            state.descriptor_manager->create_layout(get_device(), layout_config));
    }

    Core::ComputePipelineConfig config;
    config.shader = shader;
    config.set_layouts = state.layouts;
    if (push_constant_size > 0) {
        config.add_push_constant(
            vk::ShaderStageFlagBits::eCompute,
            static_cast<uint32_t>(push_constant_size));
    }

    state.pipeline = std::make_shared<Core::VKComputePipeline>();
    state.pipeline->create(get_device(), config);
    state.layout = state.pipeline->get_layout();

    return id;
}

PipelineID ShaderFoundry::create_compute_pipeline(
    ShaderID shader_id,
    const std::vector<std::vector<DescriptorBindingConfig>>& descriptor_sets,
    size_t push_constant_size)
{
    auto shader_it = m_shaders.find(shader_id);
    if (shader_it == m_shaders.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Invalid shader ID: {}", shader_id);
        return INVALID_PIPELINE;
    }

    // Convert DescriptorBindingConfig to DescriptorBindingConfig
    std::vector<std::vector<DescriptorBindingConfig>> core_descriptor_sets;
    for (const auto& set : descriptor_sets) {
        std::vector<DescriptorBindingConfig> core_set;
        for (const auto& binding : set) {
            DescriptorBindingConfig core_binding;
            core_binding.set = binding.set;
            core_binding.binding = binding.binding;
            core_binding.type = binding.type;
            core_set.push_back(core_binding);
        }
        core_descriptor_sets.push_back(core_set);
    }

    // Use existing implementation
    return create_compute_pipeline(
        shader_it->second.module,
        core_descriptor_sets,
        push_constant_size);
}
void ShaderFoundry::destroy_pipeline(PipelineID pipeline_id)
{
    auto it = m_pipelines.find(pipeline_id);
    if (it != m_pipelines.end()) {
        it->second.pipeline->cleanup(get_device());
        m_pipelines.erase(it);
    }
}

//==============================================================================
// Descriptor Management
//==============================================================================

DescriptorSetID ShaderFoundry::allocate_descriptor_set(
    PipelineID pipeline_id,
    uint32_t set_index)
{
    auto pipeline_it = m_pipelines.find(pipeline_id);
    if (pipeline_it == m_pipelines.end()) {
        return INVALID_DESCRIPTOR_SET;
    }

    PipelineState& pipeline_state = pipeline_it->second;
    if (set_index >= pipeline_state.layouts.size()) {
        return INVALID_DESCRIPTOR_SET;
    }

    DescriptorSetID id = m_next_descriptor_set_id++;

    DescriptorSetState& state = m_descriptor_sets[id];
    state.descriptor_set = pipeline_state.descriptor_manager->allocate_set(
        get_device(),
        pipeline_state.layouts[set_index]);
    state.owner_pipeline = pipeline_id;

    return id;
}

void ShaderFoundry::update_descriptor_buffer(
    DescriptorSetID descriptor_set_id,
    uint32_t binding,
    vk::Buffer buffer,
    size_t offset,
    size_t size)
{
    auto it = m_descriptor_sets.find(descriptor_set_id);
    if (it == m_descriptor_sets.end()) {
        return;
    }

    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = size;

    vk::WriteDescriptorSet write;
    write.dstSet = it->second.descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo = &buffer_info;

    get_device().updateDescriptorSets(1, &write, 0, nullptr);
}

//==============================================================================
// Command Recording
//==============================================================================

CommandBufferID ShaderFoundry::begin_compute_commands()
{
    auto& cmd_manager = m_backend->get_command_manager();

    CommandBufferID id = m_next_command_buffer_id++;

    CommandBufferState& state = m_command_buffers[id];
    state.cmd = cmd_manager.begin_single_time_commands();
    state.is_active = true;

    return id;
}

void ShaderFoundry::bind_pipeline(CommandBufferID cmd_id, PipelineID pipeline_id)
{
    auto cmd_it = m_command_buffers.find(cmd_id);
    auto pipeline_it = m_pipelines.find(pipeline_id);

    if (cmd_it == m_command_buffers.end() || pipeline_it == m_pipelines.end()) {
        return;
    }

    pipeline_it->second.pipeline->bind(cmd_it->second.cmd);
}

void ShaderFoundry::bind_descriptor_sets(
    CommandBufferID cmd_id,
    PipelineID pipeline_id,
    const std::vector<DescriptorSetID>& descriptor_set_ids)
{
    auto cmd_it = m_command_buffers.find(cmd_id);
    auto pipeline_it = m_pipelines.find(pipeline_id);

    if (cmd_it == m_command_buffers.end() || pipeline_it == m_pipelines.end()) {
        return;
    }

    std::vector<vk::DescriptorSet> vk_sets;
    for (auto ds_id : descriptor_set_ids) {
        auto ds_it = m_descriptor_sets.find(ds_id);
        if (ds_it != m_descriptor_sets.end()) {
            vk_sets.push_back(ds_it->second.descriptor_set);
        }
    }

    pipeline_it->second.pipeline->bind_descriptor_sets(cmd_it->second.cmd, vk_sets);
}

void ShaderFoundry::push_constants(
    CommandBufferID cmd_id,
    PipelineID pipeline_id,
    const void* data,
    size_t size)
{
    auto cmd_it = m_command_buffers.find(cmd_id);
    auto pipeline_it = m_pipelines.find(pipeline_id);

    if (cmd_it == m_command_buffers.end() || pipeline_it == m_pipelines.end()) {
        return;
    }

    pipeline_it->second.pipeline->push_constants(
        cmd_it->second.cmd,
        vk::ShaderStageFlagBits::eCompute,
        0,
        static_cast<uint32_t>(size),
        data);
}

void ShaderFoundry::dispatch(
    CommandBufferID cmd_id,
    uint32_t group_x,
    uint32_t group_y,
    uint32_t group_z)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it != m_command_buffers.end()) {
        it->second.cmd.dispatch(group_x, group_y, group_z);
    }
}

void ShaderFoundry::buffer_barrier(CommandBufferID cmd_id, vk::Buffer buffer)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return;
    }

    vk::BufferMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    it->second.cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags {},
        0, nullptr,
        1, &barrier,
        0, nullptr);
}

void ShaderFoundry::submit_commands(CommandBufferID cmd_id)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end() || !it->second.is_active) {
        return;
    }

    auto& cmd_manager = m_backend->get_command_manager();

    it->second.cmd.end();

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &it->second.cmd;

    vk::Queue compute_queue = m_backend->get_context().get_compute_queue();

    compute_queue.submit(1, &submit_info, nullptr);
    compute_queue.waitIdle(); // Wait for completion

    cmd_manager.free_command_buffer(it->second.cmd);

    it->second.is_active = false;
    m_command_buffers.erase(it);
}

//==============================================================================
// Utilities
//==============================================================================

vk::ShaderStageFlagBits ShaderFoundry::to_vulkan_stage(ShaderStage stage)
{
    switch (stage) {
    case ShaderStage::COMPUTE:
        return vk::ShaderStageFlagBits::eCompute;
    case ShaderStage::VERTEX:
        return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::FRAGMENT:
        return vk::ShaderStageFlagBits::eFragment;
    case ShaderStage::GEOMETRY:
        return vk::ShaderStageFlagBits::eGeometry;
    case ShaderStage::TESS_CONTROL:
        return vk::ShaderStageFlagBits::eTessellationControl;
    case ShaderStage::TESS_EVALUATION:
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
    default:
        return vk::ShaderStageFlagBits::eCompute;
    }
}

std::optional<ShaderStage> ShaderFoundry::detect_stage_from_extension(const std::string& filepath)
{
    auto vk_stage = Core::VKShaderModule::detect_stage_from_extension(filepath);
    if (!vk_stage.has_value()) {
        return std::nullopt;
    }

    switch (*vk_stage) {
    case vk::ShaderStageFlagBits::eCompute:
        return ShaderStage::COMPUTE;
    case vk::ShaderStageFlagBits::eVertex:
        return ShaderStage::VERTEX;
    case vk::ShaderStageFlagBits::eFragment:
        return ShaderStage::FRAGMENT;
    case vk::ShaderStageFlagBits::eGeometry:
        return ShaderStage::GEOMETRY;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return ShaderStage::TESS_CONTROL;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return ShaderStage::TESS_EVALUATION;
    default:
        return std::nullopt;
    }
}

//==============================================================================
// Private Helpers
//==============================================================================

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::create_shader_module()
{
    return std::make_shared<Core::VKShaderModule>();
}

vk::Device ShaderFoundry::get_device() const
{
    return m_backend->get_context().get_device();
}

} // namespace MayaFlux::Portal::Graphics
