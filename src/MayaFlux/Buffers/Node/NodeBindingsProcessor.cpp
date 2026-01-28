#include "NodeBindingsProcessor.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void NodeBindingsProcessor::bind_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    uint32_t offset,
    size_t size,
    ProcessingMode mode)
{
    auto [it, inserted] = m_bindings.try_emplace(name);
    auto& binding = it->second;

    binding.node = node;
    binding.push_constant_offset = offset;
    binding.size = size;
    binding.processing_mode.store(mode, std::memory_order_release);
}

void NodeBindingsProcessor::unbind_node(const std::string& name)
{
    m_bindings.erase(name);
}

bool NodeBindingsProcessor::has_binding(const std::string& name) const
{
    return m_bindings.contains(name);
}

std::vector<std::string> NodeBindingsProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

void NodeBindingsProcessor::set_processing_mode(const std::string& name, ProcessingMode mode)
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        it->second.processing_mode.store(mode, std::memory_order_release);
    }
}

void NodeBindingsProcessor::set_processing_mode(ProcessingMode mode)
{
    for (auto& [name, binding] : m_bindings) {
        binding.processing_mode.store(mode, std::memory_order_release);
    }
}

NodeBindingsProcessor::ProcessingMode NodeBindingsProcessor::get_processing_mode(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second.processing_mode.load(std::memory_order_acquire);
    }
    return ProcessingMode::INTERNAL;
}

void NodeBindingsProcessor::execute_shader(const std::shared_ptr<VKBuffer>& buffer)
{
    update_push_constants_from_nodes();

    auto& staging = buffer->get_pipeline_context().push_constant_staging;

    for (const auto& [name, binding] : m_bindings) {
        size_t end_offset = binding.push_constant_offset + binding.size;

        if (staging.size() < end_offset) {
            staging.resize(end_offset);
        }

        std::memcpy(
            staging.data() + binding.push_constant_offset,
            m_push_constant_data.data() + binding.push_constant_offset,
            binding.size);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NodeBindingsProcessor: Merged binding '{}' at offset {} ({} bytes)",
            name, binding.push_constant_offset, binding.size);
    }
}

void NodeBindingsProcessor::update_push_constants_from_nodes()
{
    if (m_bindings.empty()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No node bindings configured for NodeBindingsProcessor");
        return;
    }

    auto& pc_data = get_push_constant_data();

    for (auto& [name, binding] : m_bindings) {
        size_t required_size = binding.push_constant_offset + binding.size;
        if (pc_data.size() < required_size) {
            pc_data.resize(required_size);
        }

        if (!binding.node) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Node binding '{}' has null node", name);
            continue;
        }

        double value {};
        ProcessingMode mode = binding.processing_mode.load(std::memory_order_acquire);

        if (mode == ProcessingMode::INTERNAL) {
            value = Buffers::extract_single_sample(binding.node);
        } else {
            value = binding.node->get_last_output();
        }

        if (binding.size == sizeof(float)) {
            auto float_val = static_cast<float>(value);
            std::memcpy(
                pc_data.data() + binding.push_constant_offset,
                &float_val,
                sizeof(float));
        } else if (binding.size == sizeof(double)) {
            std::memcpy(
                pc_data.data() + binding.push_constant_offset,
                &value,
                sizeof(double));
        }
    }
}

} // namespace MayaFlux::Buffers
