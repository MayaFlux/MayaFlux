#pragma once

#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

class NodeGraphManager {
public:
    NodeGraphManager();

    RootNode& get_root_node(unsigned int channel = 0);

    inline const std::unordered_map<unsigned int, std::shared_ptr<RootNode>>& get_all_channel_root_nodes() const
    {
        return m_channel_root_nodes;
    }

    template <typename NodeType, typename... Args>
    inline std::shared_ptr<NodeType> create_node(const std::string& id, Args&&... args)
    {
        auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
        m_Node_registry[id] = node;
        return node;
    }

    std::shared_ptr<Node> get_node(const std::string& id);

    void connect(const std::string& source_id, const std::string& target_id);

    void connect(std::shared_ptr<Node> source, std::shared_ptr<Node> target);

    void add_to_root(const std::string& node_id, unsigned int channel = 0);
    void add_to_root(std::shared_ptr<Node> node, unsigned int channel = 0);

    bool is_node_registered(std::shared_ptr<Node> node);

private:
    std::unordered_map<unsigned int, std::shared_ptr<RootNode>> m_channel_root_nodes;
    std::unordered_map<std::string, std::shared_ptr<Node>> m_Node_registry;
};

}
