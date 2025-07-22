#include "Node.hpp"

namespace MayaFlux::Nodes {

void Node::on_tick(NodeHook callback)
{
    safe_add_callback(m_callbacks, callback);
}

void Node::on_tick_if(NodeHook callback, NodeCondition condition)
{
    safe_add_conditional_callback(m_conditional_callbacks, callback, condition);
}

bool Node::remove_hook(const NodeHook& callback)
{
    return safe_remove_callback(m_callbacks, callback);
}

bool Node::remove_conditional_hook(const NodeCondition& callback)
{
    return safe_remove_conditional_callback(m_conditional_callbacks, callback);
}

void Node::remove_all_hooks()
{
    m_callbacks.clear();
    m_conditional_callbacks.clear();
}

}
