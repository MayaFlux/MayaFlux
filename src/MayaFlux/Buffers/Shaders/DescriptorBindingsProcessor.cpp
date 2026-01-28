#include "DescriptorBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

DescriptorBindingsProcessor::DescriptorBindingsProcessor(const std::string& shader_path)
    : ShaderProcessor(shader_path)
{
}

DescriptorBindingsProcessor::DescriptorBindingsProcessor(ShaderConfig config)
    : ShaderProcessor(std::move(config))
{
}

//==============================================================================
// Binding Methods
//==============================================================================

void DescriptorBindingsProcessor::bind_scalar_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type,
    ProcessingMode mode)
{
    if (!node) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null node '{}'", name);
        return;
    }

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Descriptor '{}' not found in shader config", descriptor_name);
        return;
    }

    const auto& binding_config = m_config.bindings[descriptor_name];

    auto gpu_buffer = create_descriptor_buffer(sizeof(float), type);

    auto [it, inserted] = m_bindings.try_emplace(name);
    auto& binding = it->second;

    binding.node = node;
    binding.descriptor_name = descriptor_name;
    binding.set_index = set;
    binding.binding_index = binding_config.binding;
    binding.type = type;
    binding.binding_type = BindingType::SCALAR;
    binding.gpu_buffer = gpu_buffer;
    binding.buffer_offset = 0;
    binding.buffer_size = sizeof(float);
    binding.processing_mode.store(mode, std::memory_order_release);

    bind_buffer(descriptor_name, gpu_buffer);

    m_needs_descriptor_rebuild = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound scalar node '{}' to descriptor '{}' (mode: {})",
        name, descriptor_name, mode == ProcessingMode::INTERNAL ? "INTERNAL" : "EXTERNAL");
}

void DescriptorBindingsProcessor::bind_vector_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type,
    ProcessingMode mode)
{
    if (!node) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null node '{}'", name);
        return;
    }

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Descriptor '{}' not found in shader config", descriptor_name);
        return;
    }

    const auto& binding_config = m_config.bindings[descriptor_name];

    size_t initial_size = 4096 * sizeof(float);
    auto gpu_buffer = create_descriptor_buffer(initial_size, type);

    auto [it, inserted] = m_bindings.try_emplace(name);
    auto& binding = it->second;

    binding.node = node;
    binding.descriptor_name = descriptor_name;
    binding.set_index = set;
    binding.binding_index = binding_config.binding;
    binding.type = type;
    binding.binding_type = BindingType::VECTOR;
    binding.gpu_buffer = gpu_buffer;
    binding.buffer_offset = 0;
    binding.buffer_size = initial_size;
    binding.processing_mode.store(mode, std::memory_order_release);

    bind_buffer(descriptor_name, gpu_buffer);

    m_needs_descriptor_rebuild = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound vector node '{}' to descriptor '{}' (mode: {})",
        name, descriptor_name, mode == ProcessingMode::INTERNAL ? "INTERNAL" : "EXTERNAL");
}

void DescriptorBindingsProcessor::bind_matrix_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type,
    ProcessingMode mode)
{
    if (!node) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null node '{}'", name);
        return;
    }

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Descriptor '{}' not found in shader config", descriptor_name);
        return;
    }

    const auto& binding_config = m_config.bindings[descriptor_name];

    size_t initial_size = static_cast<long>(1024) * 1024 * sizeof(float);
    auto gpu_buffer = create_descriptor_buffer(initial_size, type);

    auto [it, inserted] = m_bindings.try_emplace(name);
    auto& binding = it->second;

    binding.node = node;
    binding.descriptor_name = descriptor_name;
    binding.set_index = set;
    binding.binding_index = binding_config.binding;
    binding.type = type;
    binding.binding_type = BindingType::MATRIX;
    binding.gpu_buffer = gpu_buffer;
    binding.buffer_offset = 0;
    binding.buffer_size = initial_size;
    binding.processing_mode.store(mode, std::memory_order_release);

    bind_buffer(descriptor_name, gpu_buffer);

    m_needs_descriptor_rebuild = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound matrix node '{}' to descriptor '{}' (mode: {})",
        name, descriptor_name, mode == ProcessingMode::INTERNAL ? "INTERNAL" : "EXTERNAL");
}

void DescriptorBindingsProcessor::bind_structured_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type,
    ProcessingMode mode)
{
    if (!node) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Cannot bind null node '{}'", name);
        return;
    }

    if (m_config.bindings.find(descriptor_name) == m_config.bindings.end()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Descriptor '{}' not found in shader config", descriptor_name);
        return;
    }

    const auto& binding_config = m_config.bindings[descriptor_name];

    size_t initial_size = static_cast<long>(1024) * 64;
    auto gpu_buffer = create_descriptor_buffer(initial_size, type);

    auto [it, inserted] = m_bindings.try_emplace(name);
    auto& binding = it->second;

    binding.node = node;
    binding.descriptor_name = descriptor_name;
    binding.set_index = set;
    binding.binding_index = binding_config.binding;
    binding.type = type;
    binding.binding_type = BindingType::STRUCTURED;
    binding.gpu_buffer = gpu_buffer;
    binding.buffer_offset = 0;
    binding.buffer_size = initial_size;
    binding.processing_mode.store(mode, std::memory_order_release);

    bind_buffer(descriptor_name, gpu_buffer);

    m_needs_descriptor_rebuild = true;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound structured node '{}' to descriptor '{}'", name, descriptor_name);
}

void DescriptorBindingsProcessor::unbind_node(const std::string& name)
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        unbind_buffer(it->second.descriptor_name);
        m_bindings.erase(it);

        m_needs_descriptor_rebuild = true;

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound node '{}'", name);
    }
}

bool DescriptorBindingsProcessor::has_binding(const std::string& name) const
{
    return m_bindings.find(name) != m_bindings.end();
}

std::vector<std::string> DescriptorBindingsProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

void DescriptorBindingsProcessor::set_processing_mode(const std::string& name, ProcessingMode mode)
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        it->second.processing_mode.store(mode, std::memory_order_release);
    }
}

void DescriptorBindingsProcessor::set_processing_mode(ProcessingMode mode)
{
    for (auto& [name, binding] : m_bindings) {
        binding.processing_mode.store(mode, std::memory_order_release);
    }
}

DescriptorBindingsProcessor::ProcessingMode DescriptorBindingsProcessor::get_processing_mode(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second.processing_mode.load(std::memory_order_acquire);
    }
    return ProcessingMode::INTERNAL;
}

//==============================================================================
// Protected Hooks
//==============================================================================

void DescriptorBindingsProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    auto& bindings_list = buffer->get_pipeline_context().descriptor_buffer_bindings;

    for (auto& [name, binding] : m_bindings) {
        update_descriptor_from_node(binding);

        bool found = false;
        for (auto& existing_binding : bindings_list) {
            if (existing_binding.set == binding.set_index && existing_binding.binding == binding.binding_index) {
                existing_binding.buffer_info.buffer = binding.gpu_buffer->get_buffer();
                existing_binding.buffer_info.offset = binding.buffer_offset;
                existing_binding.buffer_info.range = binding.buffer_size;
                found = true;
                break;
            }
        }

        if (!found) {
            bindings_list.push_back({ .set = binding.set_index,
                .binding = binding.binding_index,
                .type = binding.type,
                .buffer_info = vk::DescriptorBufferInfo {
                    binding.gpu_buffer->get_buffer(),
                    binding.buffer_offset,
                    binding.buffer_size } });
        }
    }
}

void DescriptorBindingsProcessor::on_pipeline_created(Portal::Graphics::ComputePipelineID pipeline_id)
{
    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Pipeline created for DescriptorBindingsProcessor (ID: {}, {} node bindings)",
        pipeline_id, m_bindings.size());

    ShaderProcessor::on_pipeline_created(pipeline_id);
}

//==============================================================================
// Private Implementation
//==============================================================================

void DescriptorBindingsProcessor::update_descriptor_from_node(DescriptorBinding& binding)
{
    if (!binding.node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Binding has null node");
        return;
    }

    float value {};

    if (binding.processing_mode.load(std::memory_order_acquire) == ProcessingMode::INTERNAL) {
        value = static_cast<float>(Buffers::extract_single_sample(binding.node));
    } else {
        value = static_cast<float>(binding.node->get_last_output());
    }
    Nodes::NodeContext& ctx = binding.node->get_last_context();

    switch (binding.binding_type) {
    case BindingType::SCALAR: {
        ensure_buffer_capacity(binding, sizeof(float));
        upload_to_gpu(&value, sizeof(float), binding.gpu_buffer, nullptr);
        break;
    }

    case BindingType::VECTOR: {
        auto* gpu_vec = dynamic_cast<Nodes::GpuVectorData*>(&ctx);
        if (!gpu_vec || !gpu_vec->has_gpu_data()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Node context does not provide GpuVectorData");
            return;
        }

        auto data = gpu_vec->gpu_data();

        ensure_buffer_capacity(binding, data.size_bytes());
        upload_to_gpu(data.data(), data.size_bytes(), binding.gpu_buffer, nullptr);
        break;
    }

    case BindingType::MATRIX: {
        auto* gpu_mat = dynamic_cast<Nodes::GpuMatrixData*>(&ctx);
        if (!gpu_mat || !gpu_mat->has_gpu_data()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Node context does not provide GpuMatrixData");
            return;
        }

        auto data = gpu_mat->gpu_data();
        ensure_buffer_capacity(binding, data.size_bytes());
        upload_to_gpu(data.data(), data.size_bytes(), binding.gpu_buffer, nullptr);
        break;
    }

    case BindingType::STRUCTURED: {
        auto* gpu_struct = dynamic_cast<Nodes::GpuStructuredData*>(&ctx);
        if (!gpu_struct || !gpu_struct->has_gpu_data()) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Node context does not provide GpuStructuredData");
            return;
        }

        auto data = gpu_struct->gpu_data();
        ensure_buffer_capacity(binding, data.size_bytes());
        upload_to_gpu(data.data(), data.size_bytes(), binding.gpu_buffer, nullptr);
        break;
    }
    }
}

void DescriptorBindingsProcessor::ensure_buffer_capacity(
    DescriptorBinding& binding,
    size_t required_size)
{
    if (required_size <= binding.gpu_buffer->get_size_bytes()) {
        return;
    }

    size_t new_size = required_size * 3 / 2;

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Resizing descriptor buffer '{}': {} → {} bytes",
        binding.descriptor_name, binding.buffer_size, new_size);

    binding.gpu_buffer->resize(new_size, false);

    binding.buffer_size = new_size;

    m_needs_descriptor_rebuild = true;
}

std::shared_ptr<VKBuffer> DescriptorBindingsProcessor::create_descriptor_buffer(
    size_t size,
    vk::DescriptorType type)
{
    VKBuffer::Usage usage {};

    if (type == vk::DescriptorType::eUniformBuffer) {
        usage = VKBuffer::Usage::UNIFORM;
    } else { // SSBO
        usage = VKBuffer::Usage::COMPUTE;
    }

    auto buffer = std::make_shared<VKBuffer>(
        size,
        usage,
        Kakshya::DataModality::UNKNOWN);

    auto buffer_service = Registry::BackendRegistry::instance()
                              .get_service<Registry::Service::BufferService>();

    if (!buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "create_descriptor_buffer requires a valid BufferService");
    }

    buffer_service->initialize_buffer(buffer);

    return buffer;
}

} // namespace MayaFlux::Buffers
