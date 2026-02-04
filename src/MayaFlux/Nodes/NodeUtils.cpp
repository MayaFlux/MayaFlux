#include "NodeUtils.hpp"

#include "Node.hpp"

#include "MayaFlux/API/Config.hpp"

namespace MayaFlux::Nodes {

bool callback_exists(const std::vector<NodeHook>& callbacks, const NodeHook& callback)
{
    return std::ranges::any_of(callbacks,
        [&callback](const NodeHook& hook) {
            return hook.target_type() == callback.target_type();
        });
}

bool conditional_callback_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeCondition& callback)
{
    return std::ranges::any_of(callbacks,
        [&callback](const std::pair<NodeHook, NodeCondition>& pair) {
            return pair.second.target_type() == callback.target_type();
        });
}

bool callback_pair_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeHook& callback, const NodeCondition& condition)
{
    return std::ranges::any_of(callbacks,
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
            it = callbacks.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    return removed;
}

void atomic_set_strong(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired)
{
    flag.compare_exchange_strong(expected, desired);
};

void atomic_set_flag_strong(std::atomic<NodeState>& flag, const NodeState& desired)
{
    auto expected = flag.load();
    flag.compare_exchange_strong(expected, desired);
};

void atomic_add_flag(std::atomic<NodeState>& state, NodeState flag)
{
    auto current = state.load();
    NodeState desired;
    do {
        desired = static_cast<NodeState>(current | flag);
    } while (!state.compare_exchange_weak(current, desired,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
}

void atomic_remove_flag(std::atomic<NodeState>& state, NodeState flag)
{
    auto current = state.load();
    NodeState desired;
    do {
        desired = static_cast<NodeState>(current & ~flag);
    } while (!state.compare_exchange_weak(current, desired,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
}

void atomic_set_flag_weak(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired)
{
    flag.compare_exchange_weak(expected, desired);
};

void atomic_inc_modulator_count(std::atomic<uint32_t>& count, int amount)
{
    count.fetch_add(amount, std::memory_order_relaxed);
}

void atomic_dec_modulator_count(std::atomic<uint32_t>& count, int amount)
{
    count.fetch_sub(amount, std::memory_order_relaxed);
}

void try_reset_processed_state(std::shared_ptr<Node> node)
{
    if (node && node->m_modulator_count.load(std::memory_order_relaxed) == 0) {
        node->reset_processed_state();
    }
}

std::vector<uint32_t> get_active_channels(const std::shared_ptr<Nodes::Node>& node, uint32_t fallback_channel)
{
    uint32_t channel_mask = node ? node->get_channel_mask().load() : 0;
    return get_active_channels(channel_mask, fallback_channel);
}

std::vector<uint32_t> get_active_channels(uint32_t channel_mask, uint32_t fallback_channel)
{
    std::vector<uint32_t> channels;

    if (channel_mask == 0) {
        channels.push_back(fallback_channel);
    } else {
        for (uint32_t channel = 0; channel < MayaFlux::Config::get_node_config().max_channels; ++channel) {
            if (channel_mask & (1ULL << channel)) {
                channels.push_back(channel);
            }
        }
    }

    return channels;
}

}
