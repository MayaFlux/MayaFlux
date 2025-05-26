#include "NodeUtils.hpp"
#include "Node.hpp"

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
            callbacks.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    return removed;
}

void atomic_set_strong(std::atomic<Utils::NodeState>& flag, Utils::NodeState& expected, const Utils::NodeState& desired)
{
    flag.compare_exchange_strong(expected, desired);
};

void atomic_set_flag_strong(std::atomic<Utils::NodeState>& flag, const Utils::NodeState& desired)
{
    auto expected = flag.load();
    flag.compare_exchange_strong(expected, desired);
};

void atomic_add_flag(std::atomic<Utils::NodeState>& state, Utils::NodeState flag)
{
    auto current = state.load();
    Utils::NodeState desired;
    do {
        desired = static_cast<Utils::NodeState>(current | flag);
    } while (!state.compare_exchange_weak(current, desired,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
}

void atomic_remove_flag(std::atomic<Utils::NodeState>& state, Utils::NodeState flag)
{
    auto current = state.load();
    Utils::NodeState desired;
    do {
        desired = static_cast<Utils::NodeState>(current & ~flag);
    } while (!state.compare_exchange_weak(current, desired,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
}

void atomic_set_flag_weak(std::atomic<Utils::NodeState>& flag, Utils::NodeState& expected, const Utils::NodeState& desired)
{
    flag.compare_exchange_weak(expected, desired);
};

void atomic_inc_modulator_count(std::atomic<u_int32_t>& count, int amount)
{
    count.fetch_add(amount, std::memory_order_relaxed);
}

void atomic_dec_modulator_count(std::atomic<u_int32_t>& count, int amount)
{
    count.fetch_sub(amount, std::memory_order_relaxed);
}

void try_reset_processed_state(std::shared_ptr<Node> node)
{
    if (node && node->m_modulator_count.load(std::memory_order_relaxed) == 0) {
        node->reset_processed_state();
    }
}
}
