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
            sample += node->process_sample(0.f);
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

double ChainNode::process_sample(double input)
{
    double sample = m_Source->process_sample(input);
    return m_Target->process_sample(sample);
}

std::vector<double> ChainNode::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = process_sample(0.f);
    }
    return output;
}

BinaryOpNode::BinaryOpNode(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs, CombineFunc func)
    : m_lhs(lhs)
    , m_rhs(rhs)
    , m_func(func)
{
}

double BinaryOpNode::process_sample(double input)
{
    double lhs_out = m_lhs->process_sample(input);
    double rhs_out = m_rhs->process_sample(input);

    return m_func(lhs_out, rhs_out);
}

std::vector<double> BinaryOpNode::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        output[i] = process_sample(0.0);
    }
    return output;
}

std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    return std::make_shared<ChainNode>(lhs, rhs);
}

std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    return std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a + b; });
}

std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    return std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a * b; });
}

}
