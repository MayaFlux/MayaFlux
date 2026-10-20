#include "NodeUtils.hpp"

#include "Node.hpp"

namespace MayaFlux::Nodes {

struct HookList::Retired {
    const std::vector<HookEntry>* entries;
    Retired* next;
};

HookList::HookList(Node& owner)
    : m_owner(&owner)
{
    owner.register_hook_list(*this);
}

HookList::~HookList()
{
    delete m_head.load();

    Retired* list = m_retired.load();
    while (list) {
        Retired* next = list->next;
        delete list->entries;
        delete list;
        list = next;
    }
}

HookList::Snapshot::~Snapshot()
{
    if (m_list) {
        m_list->m_readers.fetch_sub(1);
    }
}

HookId HookList::next_id() const
{
    return m_owner->next_hook_id();
}

HookList::Snapshot HookList::snapshot() const
{
    if (m_head.load(std::memory_order_relaxed) == nullptr) {
        return {};
    }

    m_readers.fetch_add(1);
    const auto* entries = m_head.load();
    if (!entries) {
        m_readers.fetch_sub(1);
        return {};
    }
    return { this, entries };
}

bool HookList::empty() const
{
    return m_head.load() == nullptr;
}

size_t HookList::size() const
{
    return snapshot().size();
}

void HookList::fire(NodeContext& ctx) const
{
    for (const auto& entry : snapshot()) {
        if (entry.condition && !(*entry.condition)(ctx)) {
            continue;
        }
        entry.hook(ctx);
    }
}

void HookList::retire(const std::vector<HookEntry>* old)
{
    auto* node = new Retired { old, m_retired.load() };
    while (!m_retired.compare_exchange_weak(node->next, node)) {
    }
}

void HookList::reclaim()
{
    Retired* list = m_retired.exchange(nullptr);
    if (!list) {
        return;
    }

    if (m_readers.load() != 0) {
        Retired* tail = list;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = m_retired.load();
        while (!m_retired.compare_exchange_weak(tail->next, list)) {
        }
        return;
    }

    while (list) {
        Retired* next = list->next;
        delete list->entries;
        delete list;
        list = next;
    }
}

bool HookList::mutate(const std::function<bool(std::vector<HookEntry>&)>& edit)
{
    for (;;) {
        m_readers.fetch_add(1);

        const std::vector<HookEntry>* current = m_head.load();

        auto next = std::make_unique<std::vector<HookEntry>>();
        if (current) {
            *next = *current;
        }

        const bool changed = edit(*next);
        bool swapped = false;
        if (changed) {
            const std::vector<HookEntry>* replacement = next->empty() ? nullptr : next.get();
            const std::vector<HookEntry>* expected = current;
            swapped = m_head.compare_exchange_strong(expected, replacement);
            if (swapped && replacement) {
                next.release();
            }
        }

        m_readers.fetch_sub(1);

        if (!changed) {
            return false;
        }
        if (swapped) {
            if (current) {
                retire(current);
            }
            reclaim();
            return true;
        }
    }
}

HookId HookList::add(NodeHook hook, const std::type_info& key, uint8_t tag, std::array<double, 2> params)
{
    const HookId id = next_id();
    mutate([&](std::vector<HookEntry>& entries) {
        entries.push_back(HookEntry { id, &key, tag, params, hook, std::nullopt });
        return true;
    });
    return id;
}

HookId HookList::add_unique(NodeHook hook, const std::type_info& key, uint8_t tag, std::array<double, 2> params)
{
    const HookId fresh = next_id();
    HookId result = fresh;
    mutate([&](std::vector<HookEntry>& entries) {
        const auto it = std::ranges::find_if(entries, [&](const HookEntry& entry) {
            return *entry.key == key && entry.params == params;
        });
        if (it != entries.end()) {
            result = it->id;
            return false;
        }
        result = fresh;
        entries.push_back(HookEntry { fresh, &key, tag, params, hook, std::nullopt });
        return true;
    });
    return result;
}

HookId HookList::add_conditional(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag)
{
    const HookId id = next_id();
    mutate([&](std::vector<HookEntry>& entries) {
        entries.push_back(HookEntry { id, &key, tag, {}, hook, condition });
        return true;
    });
    return id;
}

HookId HookList::add_conditional_unique(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag)
{
    const HookId fresh = next_id();
    HookId result = fresh;
    mutate([&](std::vector<HookEntry>& entries) {
        const auto it = std::ranges::find_if(entries, [&](const HookEntry& entry) {
            return *entry.key == key && entry.condition && entry.condition->target_type() == condition.target_type();
        });
        if (it != entries.end()) {
            result = it->id;
            return false;
        }
        result = fresh;
        entries.push_back(HookEntry { fresh, &key, tag, {}, hook, condition });
        return true;
    });
    return result;
}

bool HookList::remove(HookId id)
{
    return mutate([id](std::vector<HookEntry>& entries) {
        return std::erase_if(entries, [id](const HookEntry& entry) { return entry.id == id; }) > 0;
    });
}

bool HookList::remove_by_key(const std::type_info& key)
{
    return mutate([&](std::vector<HookEntry>& entries) {
        return std::erase_if(entries, [&](const HookEntry& entry) { return *entry.key == key; }) > 0;
    });
}

bool HookList::remove_first_by_key(const std::type_info& key)
{
    return mutate([&](std::vector<HookEntry>& entries) {
        const auto it = std::ranges::find_if(entries, [&](const HookEntry& entry) { return *entry.key == key; });
        if (it == entries.end()) {
            return false;
        }
        entries.erase(it);
        return true;
    });
}

bool HookList::remove_by_condition_key(const std::type_info& key)
{
    return mutate([&](std::vector<HookEntry>& entries) {
        return std::erase_if(entries, [&](const HookEntry& entry) {
            return entry.condition && entry.condition->target_type() == key;
        }) > 0;
    });
}

bool HookList::remove_by_tag(uint8_t tag)
{
    return mutate([tag](std::vector<HookEntry>& entries) {
        return std::erase_if(entries, [tag](const HookEntry& entry) { return entry.tag == tag; }) > 0;
    });
}

void HookList::clear()
{
    mutate([](std::vector<HookEntry>& entries) {
        if (entries.empty()) {
            return false;
        }
        entries.clear();
        return true;
    });
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
