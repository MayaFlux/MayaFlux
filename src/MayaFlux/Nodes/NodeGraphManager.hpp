#pragma once

#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

class NodeGraphManager {
public:
    NodeGraphManager();

    inline RootNode& get_root_node()
    {
        return *m_RootNode;
    }

    template <typename NodeType, typename... Args>
    std::shared_ptr<NodeType> create_node(const std::string& id, Args&&... args);

    std::shared_ptr<Node> get_node(const std::string& id);

    void connect(const std::string& source_id, const std::string& target_id);

    void add_to_root(const std::string& node_id);

    void add_to_root(std::shared_ptr<Node> node);

private:
    std::shared_ptr<RootNode> m_RootNode;
    std::unordered_map<std::string, std::shared_ptr<Node>> m_Node_registry;
};

}
