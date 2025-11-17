#include "DescriptorBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {

DescriptorBindingsProcessor::DescriptorBindingsProcessor(
    const std::string& shader_path,
    uint32_t workgroup_x)
    : ShaderProcessor(shader_path, workgroup_x)
{
}

DescriptorBindingsProcessor::DescriptorBindingsProcessor(
    ShaderProcessorConfig config)
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
    vk::DescriptorType type)
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

    m_bindings[name] = DescriptorBinding {
        .node = node,
        .descriptor_name = descriptor_name,
        .set_index = set,
        .binding_index = binding_config.binding,
        .type = type,
        .binding_type = BindingType::SCALAR,
        .gpu_buffer = gpu_buffer,
        .buffer_offset = 0,
        .buffer_size = sizeof(float)
    };

    bind_buffer(descriptor_name, gpu_buffer);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound scalar node '{}' to descriptor '{}'", name, descriptor_name);
}

void DescriptorBindingsProcessor::bind_vector_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type)
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

    m_bindings[name] = DescriptorBinding {
        .node = node,
        .descriptor_name = descriptor_name,
        .set_index = set,
        .binding_index = binding_config.binding,
        .type = type,
        .binding_type = BindingType::VECTOR,
        .gpu_buffer = gpu_buffer,
        .buffer_offset = 0,
        .buffer_size = initial_size
    };

    bind_buffer(descriptor_name, gpu_buffer);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound vector node '{}' to descriptor '{}'", name, descriptor_name);
}

void DescriptorBindingsProcessor::bind_matrix_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type)
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

    m_bindings[name] = DescriptorBinding {
        .node = node,
        .descriptor_name = descriptor_name,
        .set_index = set,
        .binding_index = binding_config.binding,
        .type = type,
        .binding_type = BindingType::MATRIX,
        .gpu_buffer = gpu_buffer,
        .buffer_offset = 0,
        .buffer_size = initial_size
    };

    bind_buffer(descriptor_name, gpu_buffer);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound matrix node '{}' to descriptor '{}'", name, descriptor_name);
}

void DescriptorBindingsProcessor::unbind_node(const std::string& name)
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        unbind_buffer(it->second.descriptor_name);
        m_bindings.erase(it);

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

//==============================================================================
// Protected Hooks
//==============================================================================

void DescriptorBindingsProcessor::on_before_dispatch(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    for (auto& [name, binding] : m_bindings) {
        update_descriptor_from_node(binding);
    }

    ShaderProcessor::on_before_dispatch(cmd_id, buffer);
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

    Nodes::NodeContext& ctx = binding.node->get_last_context();

    switch (binding.binding_type) {
    case BindingType::SCALAR: {
        auto value = static_cast<float>(binding.node->get_last_output());
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

void DescriptorBindingsProcessor::bind_structured_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set,
    vk::DescriptorType type)
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

    m_bindings[name] = DescriptorBinding {
        .node = node,
        .descriptor_name = descriptor_name,
        .set_index = set,
        .binding_index = binding_config.binding,
        .type = type,
        .binding_type = BindingType::STRUCTURED,
        .gpu_buffer = gpu_buffer,
        .buffer_offset = 0,
        .buffer_size = initial_size
    };

    bind_buffer(descriptor_name, gpu_buffer);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound structured node '{}' to descriptor '{}'", name, descriptor_name);
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

    return std::make_shared<VKBuffer>(
        size,
        usage,
        Kakshya::DataModality::UNKNOWN);
}

} // namespace MayaFlux::Buffers
