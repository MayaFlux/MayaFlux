#include "ShaderFoundry.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/BackendResoureManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKDescriptorManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKShaderModule.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Graphics {

bool ShaderFoundry::s_initialized = false;

bool ShaderFoundry::initialize(
    const std::shared_ptr<Core::VulkanBackend>& backend,
    const ShaderCompilerConfig& config)
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry already initialized (static flag)");
        return true;
    }

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

    m_global_descriptor_manager = std::make_shared<Core::VKDescriptorManager>();
    m_global_descriptor_manager->initialize(get_device(), 1024);

    m_graphics_queue = m_backend->get_context().get_graphics_queue();
    m_compute_queue = m_backend->get_context().get_compute_queue();
    m_transfer_queue = m_backend->get_context().get_transfer_queue();

    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "ShaderFoundry initialized");
    return true;
}

void ShaderFoundry::shutdown()
{
    if (!s_initialized) {
        return;
    }

    if (!m_backend) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Shutting down ShaderFoundry...");

    auto& cmd_manager = m_backend->get_command_manager();
    auto device = get_device();

    for (auto& [id, state] : m_command_buffers) {
        if (state.is_active) {
            cmd_manager.free_command_buffer(state.cmd);
        }
        if (state.timestamp_pool) {
            device.destroyQueryPool(state.timestamp_pool);
        }
    }
    m_command_buffers.clear();

    for (auto& [id, state] : m_fences) {
        device.destroyFence(state.fence);
    }
    m_fences.clear();

    for (auto& [id, state] : m_semaphores) {
        device.destroySemaphore(state.semaphore);
    }
    m_semaphores.clear();

    m_descriptor_sets.clear();

    m_global_descriptor_manager->cleanup(device);
    m_shader_cache.clear();
    m_shaders.clear();
    m_shader_filepath_cache.clear();
    m_descriptor_sets.clear();

    m_backend = nullptr;

    s_initialized = false;

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
    const std::string& content,
    std::optional<ShaderStage> stage,
    const std::string& entry_point)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "ShaderFoundry not initialized");
        return INVALID_SHADER;
    }

    DetectedSourceType source_type = detect_source_type(content);

    std::string cache_key;
    if (source_type == DetectedSourceType::FILE_GLSL || source_type == DetectedSourceType::FILE_SPIRV) {
        cache_key = content;
    } else {
        cache_key = generate_source_cache_key(content, stage.value_or(ShaderStage::COMPUTE));
    }

    auto id_it = m_shader_filepath_cache.find(cache_key);
    if (id_it != m_shader_filepath_cache.end()) {
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Using cached shader ID for: {}", cache_key);
        return id_it->second;
    }

    if (!stage.has_value()) {
        if (source_type == DetectedSourceType::FILE_GLSL || source_type == DetectedSourceType::FILE_SPIRV) {
            if (source_type == DetectedSourceType::FILE_SPIRV) {
                std::filesystem::path p(content);
                std::string stem = p.stem().string();
                stage = detect_stage_from_extension(stem);
            } else {
                stage = detect_stage_from_extension(content);
            }
        }

        if (!stage.has_value()) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
                "Cannot auto-detect shader stage from '{}' - must specify explicitly",
                content);
            return INVALID_SHADER;
        }
    }

    std::shared_ptr<Core::VKShaderModule> shader_module;

    switch (source_type) {
    case DetectedSourceType::FILE_GLSL:
        shader_module = compile_from_file(content, stage, entry_point);
        break;
    case DetectedSourceType::FILE_SPIRV:
        shader_module = compile_from_spirv(content, *stage, entry_point);
        break;
    case DetectedSourceType::SOURCE_STRING:
        shader_module = compile_from_source(content, *stage, entry_point);
        break;
    default:
        MF_ERROR(Journal::Component::Portal, Journal::Context::ShaderCompilation,
            "Cannot determine shader source type");
        return INVALID_SHADER;
    }

    if (!shader_module) {
        return INVALID_SHADER;
    }

    ShaderID id = m_next_shader_id++;

    ShaderState& state = m_shaders[id];
    state.module = shader_module;
    state.filepath = cache_key;
    state.stage = *stage;
    state.entry_point = entry_point;

    m_shader_filepath_cache[cache_key] = id;

    MF_INFO(Journal::Component::Portal, Journal::Context::ShaderCompilation,
        "Shader loaded: {} (ID: {}, stage: {})",
        cache_key, id, static_cast<int>(*stage));

    return id;
}

ShaderID ShaderFoundry::load_shader(const ShaderSource& source)
{
    return load_shader(source.content, source.stage, source.entry_point);
}

std::optional<std::filesystem::path> ShaderFoundry::resolve_shader_path(const std::string& filepath) const
{
    namespace fs = std::filesystem;

    fs::path path(filepath);

    if (path.is_absolute() || fs::exists(filepath)) {
        return path;
    }

    std::vector<std::string> search_paths = {
        Core::SHADER_BUILD_OUTPUT_DIR,
        Core::SHADER_INSTALL_DIR,
        Core::SHADER_SOURCE_DIR,
        "./shaders",
        "../shaders"
    };

    for (const auto& search_path : search_paths) {
        fs::path full_path = fs::path(search_path) / filepath;
        if (fs::exists(full_path)) {
            return full_path;
        }
    }

    return std::nullopt;
}

ShaderFoundry::DetectedSourceType ShaderFoundry::detect_source_type(const std::string& content) const
{
    auto resolved_path = resolve_shader_path(content);

    if (resolved_path.has_value()) {
        std::string ext = resolved_path->extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);

        if (ext == ".spv") {
            return DetectedSourceType::FILE_SPIRV;
        }

        return DetectedSourceType::FILE_GLSL;
    }

    if (content.size() > 1024 || content.find('\n') != std::string::npos) {
        return DetectedSourceType::SOURCE_STRING;
    }

    return DetectedSourceType::SOURCE_STRING;
}

std::string ShaderFoundry::generate_source_cache_key(const std::string& source, ShaderStage stage) const
{
    std::hash<std::string> hasher;
    size_t hash = hasher(source + std::to_string(static_cast<int>(stage)));
    return "source_" + std::to_string(hash);
}

ShaderID ShaderFoundry::reload_shader(const std::string& filepath)
{
    invalidate_cache(filepath);

    auto cache_it = m_shader_filepath_cache.find(filepath);
    if (cache_it != m_shader_filepath_cache.end()) {
        destroy_shader(cache_it->second);
        m_shader_filepath_cache.erase(cache_it);
    }

    return load_shader(filepath);
}

void ShaderFoundry::destroy_shader(ShaderID shader_id)
{
    auto it = m_shaders.find(shader_id);
    if (it != m_shaders.end()) {
        if (!it->second.filepath.empty()) {
            m_shader_filepath_cache.erase(it->second.filepath);
        }

        if (!it->second.filepath.empty()) {
            m_shader_cache.erase(it->second.filepath);
        }

        m_shaders.erase(it);
    }
}

//==============================================================================
// Shader Introspection
//==============================================================================

ShaderReflectionInfo ShaderFoundry::get_shader_reflection(ShaderID shader_id)
{
    auto it = m_shaders.find(shader_id);
    if (it == m_shaders.end()) {
        return {};
    }

    const auto& reflection = it->second.module->get_reflection();

    ShaderReflectionInfo info;
    info.stage = it->second.stage;
    info.entry_point = it->second.entry_point;
    info.workgroup_size = reflection.workgroup_size;

    for (const auto& binding : reflection.bindings) {
        DescriptorBindingInfo binding_info;
        binding_info.set = binding.set;
        binding_info.binding = binding.binding;
        binding_info.type = binding.type;
        binding_info.name = binding.name;
        info.descriptor_bindings.push_back(binding_info);
    }

    for (const auto& pc : reflection.push_constants) {
        PushConstantRangeInfo pc_info;
        pc_info.offset = pc.offset;
        pc_info.size = pc.size;
        info.push_constant_ranges.push_back(pc_info);
    }

    return info;
}

ShaderStage ShaderFoundry::get_shader_stage(ShaderID shader_id)
{
    auto it = m_shaders.find(shader_id);
    if (it != m_shaders.end()) {
        return it->second.stage;
    }
    return ShaderStage::COMPUTE;
}

std::string ShaderFoundry::get_shader_entry_point(ShaderID shader_id)
{
    auto it = m_shaders.find(shader_id);
    if (it != m_shaders.end()) {
        return it->second.entry_point;
    }
    return "main";
}

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
// Descriptor Management
//==============================================================================

DescriptorSetID ShaderFoundry::allocate_descriptor_set(vk::DescriptorSetLayout layout)
{
    DescriptorSetID id = m_next_descriptor_set_id++;

    DescriptorSetState& state = m_descriptor_sets[id];
    state.descriptor_set = m_global_descriptor_manager->allocate_set(get_device(), layout);

    return id;
}

void ShaderFoundry::update_descriptor_buffer(
    DescriptorSetID descriptor_set_id,
    uint32_t binding,
    vk::DescriptorType type,
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
    write.descriptorType = type;
    write.pBufferInfo = &buffer_info;

    get_device().updateDescriptorSets(1, &write, 0, nullptr);
}

void ShaderFoundry::update_descriptor_image(
    DescriptorSetID descriptor_set_id,
    uint32_t binding,
    vk::ImageView image_view,
    vk::Sampler sampler,
    vk::ImageLayout layout)
{
    auto it = m_descriptor_sets.find(descriptor_set_id);
    if (it == m_descriptor_sets.end()) {
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = layout;

    vk::WriteDescriptorSet write;
    write.dstSet = it->second.descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &image_info;

    get_device().updateDescriptorSets(1, &write, 0, nullptr);
}

void ShaderFoundry::update_descriptor_storage_image(
    DescriptorSetID descriptor_set_id,
    uint32_t binding,
    vk::ImageView image_view,
    vk::ImageLayout layout)
{
    auto it = m_descriptor_sets.find(descriptor_set_id);
    if (it == m_descriptor_sets.end()) {
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.imageLayout = layout;

    vk::WriteDescriptorSet write;
    write.dstSet = it->second.descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageImage;
    write.pImageInfo = &image_info;

    get_device().updateDescriptorSets(1, &write, 0, nullptr);
}

vk::DescriptorSet ShaderFoundry::get_descriptor_set(DescriptorSetID descriptor_set_id)
{
    auto it = m_descriptor_sets.find(descriptor_set_id);
    if (it == m_descriptor_sets.end()) {
        error<std::invalid_argument>(
            Journal::Component::Portal,
            Journal::Context::ShaderCompilation,
            std::source_location::current(),
            "Invalid DescriptorSetID: {}", descriptor_set_id);
    }
    return it->second.descriptor_set;
}

//==============================================================================
// Command Recording
//==============================================================================

CommandBufferID ShaderFoundry::begin_commands(CommandBufferType type)
{
    auto& cmd_manager = m_backend->get_command_manager();

    CommandBufferID id = m_next_command_id++;

    CommandBufferState& state = m_command_buffers[id];
    state.cmd = cmd_manager.begin_single_time_commands();
    state.type = type;
    state.is_active = true;

    return id;
}

vk::CommandBuffer ShaderFoundry::get_command_buffer(CommandBufferID cmd_id)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it != m_command_buffers.end()) {
        return it->second.cmd;
    }
    return nullptr;
}

//==============================================================================
// Synchronization
//==============================================================================

void ShaderFoundry::submit_and_wait(CommandBufferID cmd_id)
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

    vk::Queue queue;
    switch (it->second.type) {
    case CommandBufferType::GRAPHICS:
        queue = m_graphics_queue;
        break;
    case CommandBufferType::COMPUTE:
        queue = m_compute_queue;
        break;
    case CommandBufferType::TRANSFER:
        queue = m_transfer_queue;
        break;
    }

    queue.submit(1, &submit_info, nullptr);
    queue.waitIdle();

    cmd_manager.free_command_buffer(it->second.cmd);

    it->second.is_active = false;
    m_command_buffers.erase(it);
}

FenceID ShaderFoundry::submit_async(CommandBufferID cmd_id)
{
    auto cmd_it = m_command_buffers.find(cmd_id);
    if (cmd_it == m_command_buffers.end() || !cmd_it->second.is_active) {
        return INVALID_FENCE;
    }

    cmd_it->second.cmd.end();

    FenceID fence_id = m_next_fence_id++;

    vk::FenceCreateInfo fence_info;
    FenceState& fence_state = m_fences[fence_id];
    fence_state.fence = get_device().createFence(fence_info);
    fence_state.signaled = false;

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_it->second.cmd;

    vk::Queue queue;
    switch (cmd_it->second.type) {
    case CommandBufferType::GRAPHICS:
        queue = m_graphics_queue;
        break;
    case CommandBufferType::COMPUTE:
        queue = m_compute_queue;
        break;
    case CommandBufferType::TRANSFER:
        queue = m_transfer_queue;
        break;
    }

    queue.submit(1, &submit_info, fence_state.fence);

    cmd_it->second.is_active = false;

    return fence_id;
}

SemaphoreID ShaderFoundry::submit_with_signal(CommandBufferID cmd_id)
{
    auto cmd_it = m_command_buffers.find(cmd_id);
    if (cmd_it == m_command_buffers.end() || !cmd_it->second.is_active) {
        return INVALID_SEMAPHORE;
    }

    cmd_it->second.cmd.end();

    SemaphoreID semaphore_id = m_next_semaphore_id++;

    vk::SemaphoreCreateInfo semaphore_info;
    SemaphoreState& semaphore_state = m_semaphores[semaphore_id];
    semaphore_state.semaphore = get_device().createSemaphore(semaphore_info);

    vk::SubmitInfo submit_info;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_it->second.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphore_state.semaphore;

    vk::Queue queue;
    switch (cmd_it->second.type) {
    case CommandBufferType::GRAPHICS:
        queue = m_graphics_queue;
        break;
    case CommandBufferType::COMPUTE:
        queue = m_compute_queue;
        break;
    case CommandBufferType::TRANSFER:
        queue = m_transfer_queue;
        break;
    }

    queue.submit(1, &submit_info, nullptr);

    cmd_it->second.is_active = false;

    return semaphore_id;
}

void ShaderFoundry::wait_for_fence(FenceID fence_id)
{
    auto it = m_fences.find(fence_id);
    if (it == m_fences.end()) {
        return;
    }

    get_device().waitForFences(1, &it->second.fence, VK_TRUE, UINT64_MAX);
    it->second.signaled = true;
}

void ShaderFoundry::wait_for_fences(const std::vector<FenceID>& fence_ids)
{
    std::vector<vk::Fence> fences;
    for (auto fence_id : fence_ids) {
        auto it = m_fences.find(fence_id);
        if (it != m_fences.end()) {
            fences.push_back(it->second.fence);
        }
    }

    if (!fences.empty()) {
        get_device().waitForFences(static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
    }

    for (auto fence_id : fence_ids) {
        auto it = m_fences.find(fence_id);
        if (it != m_fences.end()) {
            it->second.signaled = true;
        }
    }
}

bool ShaderFoundry::is_fence_signaled(FenceID fence_id)
{
    auto it = m_fences.find(fence_id);
    if (it == m_fences.end()) {
        return false;
    }

    if (it->second.signaled) {
        return true;
    }

    auto result = get_device().getFenceStatus(it->second.fence);
    it->second.signaled = (result == vk::Result::eSuccess);
    return it->second.signaled;
}

CommandBufferID ShaderFoundry::begin_commands_with_wait(
    CommandBufferType type,
    SemaphoreID wait_semaphore,
    vk::PipelineStageFlags /*wait_stage*/)
{
    auto sem_it = m_semaphores.find(wait_semaphore);
    if (sem_it == m_semaphores.end()) {
        return INVALID_COMMAND_BUFFER;
    }

    return begin_commands(type);
}

vk::Semaphore ShaderFoundry::get_semaphore_handle(SemaphoreID semaphore_id)
{
    auto it = m_semaphores.find(semaphore_id);
    if (it != m_semaphores.end()) {
        return it->second.semaphore;
    }
    return nullptr;
}

//==============================================================================
// Memory Barriers
//==============================================================================

void ShaderFoundry::buffer_barrier(
    CommandBufferID cmd_id,
    vk::Buffer buffer,
    vk::AccessFlags src_access,
    vk::AccessFlags dst_access,
    vk::PipelineStageFlags src_stage,
    vk::PipelineStageFlags dst_stage)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return;
    }

    vk::BufferMemoryBarrier barrier;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    it->second.cmd.pipelineBarrier(
        src_stage,
        dst_stage,
        vk::DependencyFlags {},
        0, nullptr,
        1, &barrier,
        0, nullptr);
}

void ShaderFoundry::image_barrier(
    CommandBufferID cmd_id,
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags src_access,
    vk::AccessFlags dst_access,
    vk::PipelineStageFlags src_stage,
    vk::PipelineStageFlags dst_stage)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return;
    }

    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    it->second.cmd.pipelineBarrier(
        src_stage,
        dst_stage,
        vk::DependencyFlags {},
        0, nullptr,
        0, nullptr,
        1, &barrier);
}

//==============================================================================
// Queue Management
//==============================================================================

vk::Queue ShaderFoundry::get_graphics_queue() const { return m_graphics_queue; }
vk::Queue ShaderFoundry::get_compute_queue() const { return m_compute_queue; }
vk::Queue ShaderFoundry::get_transfer_queue() const { return m_transfer_queue; }

//==============================================================================
// Profiling
//==============================================================================

void ShaderFoundry::begin_timestamp(CommandBufferID cmd_id, const std::string& label)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return;
    }

    if (!it->second.timestamp_pool) {
        vk::QueryPoolCreateInfo pool_info;
        pool_info.queryType = vk::QueryType::eTimestamp;
        pool_info.queryCount = 128;
        it->second.timestamp_pool = get_device().createQueryPool(pool_info);
    }

    auto query_index = static_cast<uint32_t>(it->second.timestamp_queries.size() * 2);
    it->second.timestamp_queries[label] = query_index;

    it->second.cmd.resetQueryPool(it->second.timestamp_pool, query_index, 2);
    it->second.cmd.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, it->second.timestamp_pool, query_index);
}

void ShaderFoundry::end_timestamp(CommandBufferID cmd_id, const std::string& label)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return;
    }

    auto query_it = it->second.timestamp_queries.find(label);
    if (query_it == it->second.timestamp_queries.end()) {
        return;
    }

    uint32_t query_index = query_it->second;
    it->second.cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, it->second.timestamp_pool, query_index + 1);
}

ShaderFoundry::TimestampResult ShaderFoundry::get_timestamp_result(CommandBufferID cmd_id, const std::string& label)
{
    auto it = m_command_buffers.find(cmd_id);
    if (it == m_command_buffers.end()) {
        return { .label = label, .duration_ns = 0, .valid = false };
    }

    auto query_it = it->second.timestamp_queries.find(label);
    if (query_it == it->second.timestamp_queries.end()) {
        return { .label = label, .duration_ns = 0, .valid = false };
    }

    if (!it->second.timestamp_pool) {
        return { .label = label, .duration_ns = 0, .valid = false };
    }

    uint32_t query_index = query_it->second;
    uint64_t timestamps[2];

    auto result = get_device().getQueryPoolResults(
        it->second.timestamp_pool,
        query_index,
        2,
        sizeof(timestamps),
        timestamps,
        sizeof(uint64_t),
        vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    if (result != vk::Result::eSuccess) {
        return { .label = label, .duration_ns = 0, .valid = false };
    }

    auto props = m_backend->get_context().get_physical_device().getProperties();
    float timestamp_period = props.limits.timestampPeriod;

    auto duration_ns = static_cast<uint64_t>((timestamps[1] - timestamps[0]) * timestamp_period);

    return { .label = label, .duration_ns = duration_ns, .valid = true };
}

//==============================================================================
// Internal Access
//==============================================================================

std::shared_ptr<Core::VKShaderModule> ShaderFoundry::get_vk_shader_module(ShaderID shader_id)
{
    auto& foundry = ShaderFoundry::instance();
    auto it = foundry.m_shaders.find(shader_id);
    if (it != foundry.m_shaders.end()) {
        return it->second.module;
    }
    return nullptr;
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
