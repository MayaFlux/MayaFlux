#include "OperatorChain.hpp"

#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

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

void OperatorChain::process(float dt, const NetworkOperator* upstream)
{
    const NetworkOperator* last = upstream;
    for (const auto& op : m_operators) {
        if (auto* gfx = dynamic_cast<GraphicsOperator*>(op.get())) {
            if (gfx->consumes_upstream()) {
                gfx->seed_from_upstream(
                    dynamic_cast<const GraphicsOperator*>(last));
            }
        }
        op->process(dt);
        last = op.get();
    }
}

std::shared_ptr<NetworkOperator> OperatorChain::get(size_t index) const
{
    if (index >= m_operators.size())
        return nullptr;
    return m_operators[index];
}

} // namespace MayaFlux::Nodes::Network
