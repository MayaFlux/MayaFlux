#include "NodeGraphManager.hpp"
#include <sstream>

namespace MayaFlux::Nodes {

NodeGraphManager::NodeGraphManager()
{
    m_channel_root_nodes[0] = std::make_shared<Nodes::RootNode>();
}

RootNode& NodeGraphManager::get_root_node(unsigned int channel)
{
    if (m_channel_root_nodes.find(channel) == m_channel_root_nodes.end()) {
        m_channel_root_nodes[channel] = std::make_shared<RootNode>();
    }
    return *m_channel_root_nodes[channel];
}

template <typename NodeType, typename... Args>
std::shared_ptr<NodeType> NodeGraphManager::create_node(const std::string& id, Args&&... args)
{
    auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
    m_Node_registry[id] = node;
    return node;
}

std::shared_ptr<Node> NodeGraphManager::get_node(const std::string& id)
{
    auto it = m_Node_registry.find(id);

    if (it != m_Node_registry.end()) {
        return it->second;
    }
    return nullptr;
}

void NodeGraphManager::connect(const std::string& source_id, const std::string& target_id)
{
    auto source = get_node(source_id);
    auto target = get_node(target_id);

    if (source && target) {
        source >> target;
    }
}

void NodeGraphManager::add_to_root(const std::string& node_id, unsigned int channel)
{
    auto node = get_node(node_id);
    if (node) {
        get_root_node(channel).register_node(node);
    }
}

void NodeGraphManager::add_to_root(std::shared_ptr<Node> node, unsigned int channel)
{
    bool found = false;
    for (const auto& pair : m_Node_registry) {
        if (pair.second == node) {
            found = true;
            break;
        }
    }

    if (!found) {
        std::stringstream ss;
        ss << "node_" << node.get();
        std::string generated_id = ss.str();
        m_Node_registry[generated_id] = node;
    }

    get_root_node(channel).register_node(node);
}
}
