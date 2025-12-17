#include "ShaderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// Construction
//==============================================================================

ShaderProcessor::ShaderProcessor(const std::string& shader_path)
    : m_config({ shader_path })
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
    initialize_compute_service();
}

ShaderProcessor::ShaderProcessor(ShaderConfig config)
    : m_config(std::move(config))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
    initialize_compute_service();
}

ShaderProcessor::~ShaderProcessor()
{
    cleanup();
}

//==============================================================================
// BufferProcessor Interface
//==============================================================================

void ShaderProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "ShaderProcessor can only process VKBuffers");
        return;
    }

    if (!m_initialized) {
        initialize_shader();
        initialize_pipeline(buffer);
        m_initialized = true;
        m_needs_descriptor_rebuild = true;
    }

    if (m_needs_pipeline_rebuild) {
        initialize_pipeline(buffer);
        m_needs_pipeline_rebuild = false;
        m_needs_descriptor_rebuild = true;
    }

    if (m_needs_descriptor_rebuild) {
        initialize_descriptors();
        m_needs_descriptor_rebuild = false;
    } else {
        update_descriptors();
    }

    execute_shader(vk_buffer);
}

void ShaderProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    if (m_config.bindings.empty()) {
        auto_bind_buffer(vk_buffer);
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "ShaderProcessor attached to VKBuffer (size: {} bytes, modality: {})",
        vk_buffer->get_size_bytes(),
        static_cast<int>(vk_buffer->get_modality()));
}

void ShaderProcessor::on_detach(std::shared_ptr<Buffer> buffer)
{
    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer)
        return;

    for (auto it = m_bound_buffers.begin(); it != m_bound_buffers.end();) {
        if (it->second == vk_buffer) {
            it = m_bound_buffers.erase(it);
        } else {
            ++it;
        }
    }
    m_needs_descriptor_rebuild = true;
}

bool ShaderProcessor::is_compatible_with(std::shared_ptr<Buffer> buffer) const
{
    return std::dynamic_pointer_cast<VKBuffer>(buffer) != nullptr;
}

//==============================================================================
// Buffer Binding
//==============================================================================

void ShaderProcessor::bind_buffer(const std::string& descriptor_name, const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null buffer to descriptor '{}'", descriptor_name);
        return;
    }

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        ShaderBinding default_binding;
        default_binding.set = 0;
        default_binding.binding = static_cast<uint32_t>(m_config.bindings.size());
        default_binding.type = vk::DescriptorType::eStorageBuffer;
        m_config.bindings[descriptor_name] = default_binding;

        // MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        //     "Created default binding for '{}': set={}, binding={}",
        //     descriptor_name, default_binding.set, default_binding.binding);
    }

    m_bound_buffers[descriptor_name] = buffer;
    m_needs_descriptor_rebuild = true;

    // MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
    //     "Bound buffer to descriptor '{}' (size: {} bytes)",
    //     descriptor_name, buffer->get_size_bytes());
}

void ShaderProcessor::unbind_buffer(const std::string& descriptor_name)
{
    auto it = m_bound_buffers.find(descriptor_name);
    if (it != m_bound_buffers.end()) {
        m_bound_buffers.erase(it);
        m_needs_descriptor_rebuild = true;
    }
}

std::shared_ptr<VKBuffer> ShaderProcessor::get_bound_buffer(const std::string& descriptor_name) const
{
    auto it = m_bound_buffers.find(descriptor_name);
    return it != m_bound_buffers.end() ? it->second : nullptr;
}

void ShaderProcessor::auto_bind_buffer(const std::shared_ptr<VKBuffer>& buffer)
{
    std::string descriptor_name;
    if (m_auto_bind_index == 0) {
        descriptor_name = "input";
    } else if (m_auto_bind_index == 1) {
        descriptor_name = "output";
    } else {
        descriptor_name = "buffer_" + std::to_string(m_auto_bind_index);
    }

    bind_buffer(descriptor_name, buffer);
    m_auto_bind_index++;
}

//==============================================================================
// Shader Management
//==============================================================================

bool ShaderProcessor::hot_reload_shader()
{
    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Hot-reloading shader: {}", m_config.shader_path);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto new_shader_id = foundry.reload_shader(m_config.shader_path);

    if (new_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Hot-reload failed for shader: {}", m_config.shader_path);
        return false;
    }

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
    }

    m_shader_id = new_shader_id;
    m_needs_pipeline_rebuild = true;
    on_shader_loaded(m_shader_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Shader hot-reloaded successfully (ID: {})", m_shader_id);
    return true;
}

void ShaderProcessor::set_shader(const std::string& shader_path)
{
    m_config.shader_path = shader_path;
    m_needs_pipeline_rebuild = true;
    initialize_shader();
}

//==============================================================================
// Push Constants
//==============================================================================

void ShaderProcessor::set_push_constant_size(size_t size)
{
    m_config.push_constant_size = size;
    m_push_constant_data.resize(size);
    m_needs_pipeline_rebuild = true;
}

void ShaderProcessor::set_push_constant_data_raw(const void* data, size_t size)
{
    if (size > m_config.push_constant_size) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Push constant data size {} exceeds configured size {}",
            size, m_config.push_constant_size);
        return;
    }

    m_push_constant_data.resize(size);
    std::memcpy(m_push_constant_data.data(), data, size);
}

//==============================================================================
// Specialization Constants
//==============================================================================

void ShaderProcessor::set_specialization_constant(uint32_t constant_id, uint32_t value)
{
    m_config.specialization_constants[constant_id] = value;
    m_needs_pipeline_rebuild = true;
}

void ShaderProcessor::clear_specialization_constants()
{
    m_config.specialization_constants.clear();
    m_needs_pipeline_rebuild = true;
}

//==============================================================================
// Configuration
//==============================================================================

void ShaderProcessor::set_config(const ShaderConfig& config)
{
    m_config = config;
    m_needs_pipeline_rebuild = true;
    m_needs_descriptor_rebuild = true;
    initialize_shader();
}

void ShaderProcessor::add_binding(const std::string& descriptor_name, const ShaderBinding& binding)
{
    m_config.bindings[descriptor_name] = binding;
    m_needs_descriptor_rebuild = true;
}

//==========================================================================
// Data movement Queries
//==========================================================================

[[nodiscard]] ShaderProcessor::BufferUsageHint ShaderProcessor::get_buffer_usage_hint(const std::string& descriptor_name) const
{
    if (descriptor_name == "input")
        return BufferUsageHint::INPUT_READ;
    if (descriptor_name == "output")
        return BufferUsageHint::OUTPUT_WRITE;
    return BufferUsageHint::NONE;
}

[[nodiscard]] bool ShaderProcessor::is_in_place_operation(const std::string& descriptor_name) const
{
    auto hint = get_buffer_usage_hint(descriptor_name);
    return hint == BufferUsageHint::BIDIRECTIONAL;
}

[[nodiscard]] bool ShaderProcessor::has_binding(const std::string& descriptor_name) const
{
    return m_config.bindings.find(descriptor_name) != m_config.bindings.end();
}

[[nodiscard]] std::vector<std::string> ShaderProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_config.bindings.size());
    for (const auto& [name, _] : m_config.bindings) {
        names.push_back(name);
    }
    return names;
}

[[nodiscard]] bool ShaderProcessor::are_bindings_complete() const
{
    for (const auto& [name, _] : m_config.bindings) {
        if (m_bound_buffers.find(name) == m_bound_buffers.end()) {
            return false;
        }
    }
    return true;
}

//==============================================================================
// Protected Hooks
//==============================================================================

void ShaderProcessor::on_before_compile(const std::string&) { }
void ShaderProcessor::on_shader_loaded(Portal::Graphics::ShaderID) { }
void ShaderProcessor::on_pipeline_created(Portal::Graphics::ComputePipelineID) { }
void ShaderProcessor::on_before_descriptors_create() { }
void ShaderProcessor::on_descriptors_created() { }
void ShaderProcessor::on_before_execute(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { }
void ShaderProcessor::on_after_execute(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { }

//==============================================================================
// Private Implementation
//==============================================================================

void ShaderProcessor::initialize_shader()
{
    on_before_compile(m_config.shader_path);

    auto& foundry = Portal::Graphics::get_shader_foundry();
    m_shader_id = foundry.load_shader(m_config.shader_path, m_config.stage, m_config.entry_point);

    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to load shader: {}", m_config.shader_path);
        return;
    }

    on_shader_loaded(m_shader_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Shader loaded: {} (ID: {})", m_config.shader_path, m_shader_id);
}

void ShaderProcessor::update_descriptors()
{
    if (m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    for (const auto& [descriptor_name, buffer] : m_bound_buffers) {
        auto binding_it = m_config.bindings.find(descriptor_name);
        if (binding_it == m_config.bindings.end()) {
            continue;
        }

        const auto& binding = binding_it->second;

        if (binding.set >= m_descriptor_set_ids.size()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Invalid descriptor set index {} for binding '{}'",
                binding.set, descriptor_name);
            continue;
        }

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[binding.set],
            binding.binding,
            binding.type,
            buffer->get_buffer(),
            0,
            buffer->get_size_bytes());
    }
}

void ShaderProcessor::cleanup()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id);
        m_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    m_descriptor_set_ids.clear();
    m_bound_buffers.clear();
    m_initialized = false;
}

} // namespace MayaFlux::Buffers
