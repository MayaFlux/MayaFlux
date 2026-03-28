#include "OperatorChain.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::Network {

void OperatorChain::add(std::shared_ptr<NetworkOperator> op)
{
    if (!op) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Init,
            "OperatorChain::add: null operator ignored");
        return;
    }
    m_operators.push_back(std::move(op));
}

void OperatorChain::remove(const std::shared_ptr<NetworkOperator>& op)
{
    auto it = std::ranges::find(m_operators, op);
    if (it != m_operators.end())
        m_operators.erase(it);
}

void OperatorChain::clear()
{
    m_operators.clear();
}

void OperatorChain::process(float dt)
{
    for (const auto& op : m_operators)
        op->process(dt);
}

std::shared_ptr<NetworkOperator> OperatorChain::get(size_t index) const
{
    if (index >= m_operators.size())
        return nullptr;
    return m_operators[index];
}

} // namespace MayaFlux::Nodes::Network
