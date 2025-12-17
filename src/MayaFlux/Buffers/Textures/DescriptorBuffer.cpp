#include "DescriptorBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

DescriptorBuffer::DescriptorBuffer(
    const ShaderConfig& config,
    size_t initial_size)
    : VKBuffer(
          initial_size,
          Usage::UNIFORM, // Can hold both UBO and SSBO
          Kakshya::DataModality::UNKNOWN)
    , m_config(config)
{
    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created DescriptorBuffer for shader '{}' ({} bytes)",
        config.shader_path,
        get_size_bytes());
}

void DescriptorBuffer::initialize()
{
    m_bindings_processor = std::make_shared<DescriptorBindingsProcessor>(m_config);
    set_default_processor(m_bindings_processor);
}

void DescriptorBuffer::bind_scalar(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set)
{
    if (!m_bindings_processor) {
        error<std::logic_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DescriptorBuffer not initialized. Call initialize() first.");
    }

    auto& binding_config = m_config.bindings.at(descriptor_name);
    m_bindings_processor->bind_scalar_node(name, node, descriptor_name, set, binding_config.type);
}

void DescriptorBuffer::bind_vector(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set)
{
    if (!m_bindings_processor) {
        error<std::logic_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DescriptorBuffer not initialized. Call initialize() first.");
    }

    auto& binding_config = m_config.bindings.at(descriptor_name);
    m_bindings_processor->bind_vector_node(name, node, descriptor_name, set, binding_config.type);
}

void DescriptorBuffer::bind_matrix(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set)
{
    if (!m_bindings_processor) {
        error<std::logic_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DescriptorBuffer not initialized. Call initialize() first.");
    }

    auto& binding_config = m_config.bindings.at(descriptor_name);
    m_bindings_processor->bind_matrix_node(name, node, descriptor_name, set, binding_config.type);
}

void DescriptorBuffer::bind_structured(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    const std::string& descriptor_name,
    uint32_t set)
{
    if (!m_bindings_processor) {
        error<std::logic_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "DescriptorBuffer not initialized. Call initialize() first.");
    }

    auto& binding_config = m_config.bindings.at(descriptor_name);
    m_bindings_processor->bind_structured_node(name, node, descriptor_name, set, binding_config.type);
}

void DescriptorBuffer::unbind(const std::string& name)
{
    if (m_bindings_processor) {
        m_bindings_processor->unbind_node(name);
    }
}

std::vector<std::string> DescriptorBuffer::get_binding_names() const
{
    return m_bindings_processor ? m_bindings_processor->get_binding_names() : std::vector<std::string> {};
}

} // namespace MayaFlux::Buffers
