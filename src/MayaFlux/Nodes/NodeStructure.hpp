#include "Node.hpp"

namespace MayaFlux::Nodes {

class RootNode {
public:
    void register_node(std::shared_ptr<Node> node);
    void unregister_node(std::shared_ptr<Node> node);

    std::vector<double> process(unsigned int num_samples = 1);

private:
    std::vector<std::shared_ptr<Node>> m_Nodes;
};

class ChainNode : public Node {
public:
    ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target);

    double processSample(double input) override;

    std::vector<double> processFull(unsigned int num_samples) override;

private:
    std::shared_ptr<Node> m_Source;
    std::shared_ptr<Node> m_Target;
};
}
