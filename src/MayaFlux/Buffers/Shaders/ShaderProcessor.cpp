#include "ShaderProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

//==============================================================================
// Construction
//==============================================================================

ShaderProcessor::ShaderProcessor(const std::string& shader_path, uint32_t workgroup_x)
    : m_config(shader_path)
{
    m_config.dispatch.workgroup_x = workgroup_x;
    initialize_buffer_service();
    initialize_compute_service();
}

ShaderProcessor::ShaderProcessor(ShaderProcessorConfig config)
    : m_config(std::move(config))
{
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
        initialize_pipeline();
        initialize_descriptors();
        m_initialized = true;
    }

    if (m_needs_pipeline_rebuild) {
        initialize_pipeline();
        m_needs_pipeline_rebuild = false;
    }

    if (m_needs_descriptor_rebuild) {
        initialize_descriptors();
        m_needs_descriptor_rebuild = false;
    }

    update_descriptors();
    execute_dispatch(vk_buffer);
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
// Dispatch Configuration
//==============================================================================

void ShaderProcessor::set_workgroup_size(uint32_t x, uint32_t y, uint32_t z)
{
    m_config.dispatch.workgroup_x = x;
    m_config.dispatch.workgroup_y = y;
    m_config.dispatch.workgroup_z = z;
}

void ShaderProcessor::set_dispatch_mode(ShaderDispatchConfig::DispatchMode mode)
{
    m_config.dispatch.mode = mode;
}

void ShaderProcessor::set_manual_dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    m_config.dispatch.mode = ShaderDispatchConfig::DispatchMode::MANUAL;
    m_config.dispatch.group_count_x = x;
    m_config.dispatch.group_count_y = y;
    m_config.dispatch.group_count_z = z;
}

void ShaderProcessor::set_custom_dispatch(
    std::function<std::array<uint32_t, 3>(const std::shared_ptr<VKBuffer>&)> calculator)
{
    m_config.dispatch.mode = ShaderDispatchConfig::DispatchMode::CUSTOM;
    m_config.dispatch.custom_calculator = std::move(calculator);
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

void ShaderProcessor::set_config(const ShaderProcessorConfig& config)
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
void ShaderProcessor::on_before_dispatch(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { }
void ShaderProcessor::on_after_dispatch(Portal::Graphics::CommandBufferID, const std::shared_ptr<VKBuffer>&) { }

std::array<uint32_t, 3> ShaderProcessor::calculate_dispatch_size(const std::shared_ptr<VKBuffer>& buffer)
{
    using DispatchMode = ShaderDispatchConfig::DispatchMode;

    switch (m_config.dispatch.mode) {
    case DispatchMode::MANUAL:
        return { m_config.dispatch.group_count_x, m_config.dispatch.group_count_y, m_config.dispatch.group_count_z };

    case DispatchMode::ELEMENT_COUNT: {
        uint64_t element_count = 0;
        const auto& dimensions = buffer->get_dimensions();

        if (!dimensions.empty()) {
            element_count = dimensions[0].size;
        } else {
            element_count = buffer->get_size_bytes() / sizeof(float);
        }

        auto groups_x = static_cast<uint32_t>(
            (element_count + m_config.dispatch.workgroup_x - 1) / m_config.dispatch.workgroup_x);
        return { groups_x, 1, 1 };
    }

    case DispatchMode::BUFFER_SIZE: {
        uint64_t size_bytes = buffer->get_size_bytes();
        auto groups_x = static_cast<uint32_t>(
            (size_bytes + m_config.dispatch.workgroup_x - 1) / m_config.dispatch.workgroup_x);
        return { groups_x, 1, 1 };
    }

    case DispatchMode::CUSTOM:
        if (m_config.dispatch.custom_calculator) {
            return m_config.dispatch.custom_calculator(buffer);
        }
        return { 1, 1, 1 };

    default:
        return { 1, 1, 1 };
    }
}

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

void ShaderProcessor::initialize_pipeline()
{
    if (m_shader_id == Portal::Graphics::INVALID_SHADER) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot create pipeline without shader");
        return;
    }

    auto& compute_press = Portal::Graphics::get_compute_press();

    std::map<uint32_t, std::vector<std::pair<std::string, ShaderBinding>>> bindings_by_set;
    for (const auto& [name, binding] : m_config.bindings) {
        bindings_by_set[binding.set].emplace_back(name, binding);
    }

    std::vector<std::vector<Portal::Graphics::DescriptorBindingConfig>> descriptor_sets;
    for (const auto& [set_index, set_bindings] : bindings_by_set) {
        std::vector<Portal::Graphics::DescriptorBindingConfig> set_config;
        for (const auto& [name, binding] : set_bindings) {
            set_config.emplace_back(binding.set, binding.binding, binding.type);
        }
        descriptor_sets.push_back(set_config);
    }

    m_pipeline_id = compute_press.create_pipeline(
        m_shader_id,
        descriptor_sets,
        m_config.push_constant_size);

    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to create compute pipeline");
        return;
    }

    on_pipeline_created(m_pipeline_id);

    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Compute pipeline created (ID: {}, {} descriptor sets, {} bytes push constants)",
        m_pipeline_id, descriptor_sets.size(), m_config.push_constant_size);
}

void ShaderProcessor::initialize_descriptors()
{
    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot allocate descriptor sets without pipeline");
        return;
    }

    on_before_descriptors_create();

    auto& compute_press = Portal::Graphics::get_compute_press();

    m_descriptor_set_ids = compute_press.allocate_pipeline_descriptors(m_pipeline_id);

    if (m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Failed to allocate descriptor sets");
        return;
    }

    update_descriptors();
    on_descriptors_created();

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Descriptor sets initialized: {} sets", m_descriptor_set_ids.size());
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

void ShaderProcessor::execute_dispatch(const std::shared_ptr<VKBuffer>& buffer)
{
    if (m_pipeline_id == Portal::Graphics::INVALID_COMPUTE_PIPELINE || m_descriptor_set_ids.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot dispatch without pipeline and descriptors");
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    auto cmd_id = foundry.begin_commands(Portal::Graphics::ShaderFoundry::CommandBufferType::COMPUTE);

    m_last_command_buffer = cmd_id;
    m_last_processed_buffer = buffer;

    compute_press.bind_pipeline(cmd_id, m_pipeline_id);

    compute_press.bind_descriptor_sets(cmd_id, m_pipeline_id, m_descriptor_set_ids);

    if (!m_push_constant_data.empty()) {
        compute_press.push_constants(
            cmd_id,
            m_pipeline_id,
            m_push_constant_data.data(),
            m_push_constant_data.size());
    }

    on_before_dispatch(cmd_id, buffer);

    auto dispatch_size = calculate_dispatch_size(buffer);
    compute_press.dispatch(cmd_id, dispatch_size[0], dispatch_size[1], dispatch_size[2]);

    on_after_dispatch(cmd_id, buffer);

    foundry.buffer_barrier(
        cmd_id,
        buffer->get_buffer(),
        vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead,
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eTransfer);

    foundry.submit_and_wait(cmd_id);
}

void ShaderProcessor::cleanup()
{
    auto& foundry = Portal::Graphics::get_shader_foundry();
    auto& compute_press = Portal::Graphics::get_compute_press();

    if (m_pipeline_id != Portal::Graphics::INVALID_COMPUTE_PIPELINE) {
        compute_press.destroy_pipeline(m_pipeline_id); // ← ComputePress
        m_pipeline_id = Portal::Graphics::INVALID_COMPUTE_PIPELINE;
    }

    if (m_shader_id != Portal::Graphics::INVALID_SHADER) {
        foundry.destroy_shader(m_shader_id); // ← ShaderFoundry
        m_shader_id = Portal::Graphics::INVALID_SHADER;
    }

    m_descriptor_set_ids.clear();
    m_bound_buffers.clear();
    m_initialized = false;
}
} // namespace MayaFlux::Buffers
