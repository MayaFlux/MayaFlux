#include "NodeOperators.hpp"
#include "NodeStructure.hpp"

namespace MayaFlux::Nodes {

std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    auto chain = std::make_shared<ChainNode>(lhs, rhs);
    chain->initialize();
    return chain;
}

std::shared_ptr<Node> operator+(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    auto result = std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a + b; });
    result->initialize();
    return result;
}

std::shared_ptr<Node> operator*(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs)
{
    auto result = std::make_shared<BinaryOpNode>(lhs, rhs, [](double a, double b) { return a * b; });
    result->initialize();
    return result;
}
}
