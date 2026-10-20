#include "NodeUtils.hpp"

#include "Node.hpp"

namespace MayaFlux::Nodes {

HookList::HookList(Node& owner)
{
    owner.register_hook_list(*this);
}

HookId HookList::add(NodeHook hook, const std::type_info& key, uint8_t tag, std::array<double, 2> params)
{
    const HookId id = m_next_id++;
    m_entries.push_back(HookEntry { id, &key, tag, params, std::move(hook), std::nullopt });
    return id;
}

HookId HookList::add_unique(NodeHook hook, const std::type_info& key, uint8_t tag, std::array<double, 2> params)
{
    const bool exists = std::ranges::any_of(m_entries, [&](const HookEntry& entry) {
        return *entry.key == key && entry.params == params;
    });
    if (exists) {
        return 0;
    }
    return add(std::move(hook), key, tag, params);
}

HookId HookList::add_conditional(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag)
{
    const HookId id = m_next_id++;
    m_entries.push_back(HookEntry { id, &key, tag, {}, std::move(hook), std::move(condition) });
    return id;
}

HookId HookList::add_conditional_unique(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag)
{
    const bool exists = std::ranges::any_of(m_entries, [&](const HookEntry& entry) {
        return *entry.key == key && entry.condition && entry.condition->target_type() == condition.target_type();
    });
    if (exists) {
        return 0;
    }
    return add_conditional(std::move(hook), std::move(condition), key, tag);
}

bool HookList::remove(HookId id)
{
    return std::erase_if(m_entries, [id](const HookEntry& entry) { return entry.id == id; }) > 0;
}

bool HookList::remove_by_key(const std::type_info& key)
{
    return std::erase_if(m_entries, [&](const HookEntry& entry) { return *entry.key == key; }) > 0;
}

bool HookList::remove_first_by_key(const std::type_info& key)
{
    const auto it = std::ranges::find_if(m_entries, [&](const HookEntry& entry) { return *entry.key == key; });
    if (it == m_entries.end()) {
        return false;
    }
    m_entries.erase(it);
    return true;
}

bool HookList::remove_by_condition_key(const std::type_info& key)
{
    return std::erase_if(m_entries, [&](const HookEntry& entry) {
        return entry.condition && entry.condition->target_type() == key;
    }) > 0;
}

bool HookList::remove_by_tag(uint8_t tag)
{
    return std::erase_if(m_entries, [tag](const HookEntry& entry) { return entry.tag == tag; }) > 0;
}

void HookList::clear()
{
    m_entries.clear();
}

void HookList::fire(NodeContext& ctx) const
{
    for (const auto& entry : m_entries) {
        if (entry.condition && !(*entry.condition)(ctx)) {
            continue;
        }
        entry.hook(ctx);
    }
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
        for (uint32_t channel = 0; channel < MAX_CHANNEL_COUNT; ++channel) {
            if (channel_mask & (1ULL << channel)) {
                channels.push_back(channel);
            }
        }
    }

    return channels;
}

void update_routing_state(RoutingState& state)
{
    state.cycles_elapsed++;

    double progress = 0.;
    if (state.fade_cycles > 0) {
        progress = std::min<double>(1.0, static_cast<double>(state.cycles_elapsed) / state.fade_cycles);
    } else {
        for (uint32_t ch = 0; ch < 32; ++ch) {
            bool will_be_in = state.to_channels & (1 << ch);
            state.amount[ch] = will_be_in ? 1.0 : 0.0;
        }
        state.phase = RoutingState::NONE;
        return;
    }

    for (uint32_t ch = 0; ch < 32; ch++) {
        bool was_in = state.from_channels & (1 << ch);
        bool will_be_in = state.to_channels & (1 << ch);

        if (was_in && will_be_in) {
            state.amount[ch] = 1.0;
        } else if (was_in && !will_be_in) {
            state.amount[ch] = 1.0 - progress;
        } else if (!was_in && will_be_in) {
            state.amount[ch] = progress;
        } else {
            state.amount[ch] = 0.0;
        }
    }

    if (state.cycles_elapsed >= state.fade_cycles) {
        state.phase = RoutingState::COMPLETED;
    }
}

}
