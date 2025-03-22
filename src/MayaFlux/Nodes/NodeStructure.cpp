#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

void RootNode::register_node(std::shared_ptr<Node> node)
{
    m_Nodes.push_back(node);
}

void RootNode::unregister_node(std::shared_ptr<Node> node)
{
    auto it = m_Nodes.begin();
    while (it != m_Nodes.end()) {
        if ((*it).get() == node.get()) {
            it = m_Nodes.erase(it);
            break;
        }
        ++it;
    }
}

std::vector<double> RootNode::process(unsigned int num_samples)
{
    std::vector<double> output(num_samples);

    for (unsigned int i = 0; i < num_samples; i++) {
        double sample = 0.f;

        for (auto& node : m_Nodes) {
            sample += node->processSample(0.f);
        }
        output[i] = sample;
    }

    return output;
}

ChainNode::ChainNode(std::shared_ptr<Node> source, std::shared_ptr<Node> target)
    : m_Source(source)
    , m_Target(target)
{
}

double ChainNode::processSample(double input)
{
    double sample = m_Source->processSample(input);
    return m_Target->processSample(sample);
}

std::vector<double> ChainNode::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = processSample(0.f);
    }
    return output;
}

std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    return std::make_shared<ChainNode>(lhs, rhs);
}
}
