#include "NodeUtils.hpp"

namespace MayaFlux::Nodes {

bool callback_exists(const std::vector<NodeHook>& callbacks, const NodeHook& callback)
{
    return std::any_of(callbacks.begin(), callbacks.end(),
        [&callback](const NodeHook& hook) {
            return hook.target_type() == callback.target_type();
        });
}

bool conditional_callback_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeCondition& callback)
{
    return std::any_of(callbacks.begin(), callbacks.end(),
        [&callback](const std::pair<NodeHook, NodeCondition>& pair) {
            return pair.second.target_type() == callback.target_type();
        });
}

bool callback_pair_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeHook& callback, const NodeCondition& condition)
{
    return std::any_of(callbacks.begin(), callbacks.end(),
        [&callback, &condition](const std::pair<NodeHook, NodeCondition>& pair) {
            return pair.first.target_type() == callback.target_type() && pair.second.target_type() == condition.target_type();
        });
}

bool safe_add_callback(std::vector<NodeHook>& callbacks, const NodeHook& callback)
{
    if (!callback_exists(callbacks, callback)) {
        callbacks.push_back(callback);
        return true;
    }
    return false;
}

bool safe_add_conditional_callback(std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeHook& callback, const NodeCondition& condition)
{
    if (!callback_pair_exists(callbacks, callback, condition)) {
        callbacks.emplace_back(callback, condition);
        return true;
    }
    return false;
}

bool safe_remove_callback(std::vector<NodeHook>& callbacks, const NodeHook& callback)
{
    bool removed = false;
    auto it = callbacks.begin();

    while (it != callbacks.end()) {
        if (it->target_type() == callback.target_type()) {
            it = callbacks.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    return removed;
}

bool safe_remove_conditional_callback(std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeCondition& callback)
{
    bool removed = false;
    auto it = callbacks.begin();

    while (it != callbacks.end()) {
        if (it->second.target_type() == callback.target_type()) {
            // it = callbacks.erase(it);
            callbacks.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    return removed;
}
}
