#include "NodeBindingsProcessor.hpp"
#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void NodeBindingsProcessor::bind_node(
    const std::string& name,
    const std::shared_ptr<Nodes::Node>& node,
    uint32_t offset,
    size_t size)
{
    m_bindings[name] = NodeBinding { .node = node, .push_constant_offset = offset, .size = size };
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

void NodeBindingsProcessor::on_before_dispatch(
    Portal::Graphics::CommandBufferID cmd_id,
    const std::shared_ptr<VKBuffer>& buffer)
{
    update_push_constants_from_nodes();

    ShaderProcessor::on_before_dispatch(cmd_id, buffer);
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

        if (!binding.node) {
            MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Node binding '{}' has null node", name);
            continue;
        }

        double value = Buffers::extract_single_sample(binding.node);

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
