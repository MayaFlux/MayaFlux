#include "ShaderFoundry.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
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
