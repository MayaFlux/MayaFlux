#pragma once

#include "NodeSpec.hpp"

namespace MayaFlux::Nodes {

class Node;
class NodeContext;

/**
 * @typedef TypedHook
 * @brief Callback function type for node processing events, parameterised on context type.
 *
 * Defaults to NodeContext so existing code is unaffected. Concrete node classes
 * specialise this with their own context type for their domain-specific callbacks,
 * eliminating the need for callers to cast inside the lambda.
 *
 * Example (base interface):
 * ```cpp
 * node->on_tick([](NodeContext& ctx) { ... });
 * ```
 * Example (concrete node):
 * ```cpp
 * phasor->on_phase_wrap([](GeneratorContext& ctx) { ... });
 * ```
 */
template <typename ContextT = NodeContext>
using TypedHook = std::function<void(ContextT&)>;

/**
 * @typedef NodeHook
 * @brief Alias for TypedHook<NodeContext>.
 *
 * Preserved for backward compatibility. All existing NodeHook usage continues
 * to compile without changes. Migrate incrementally to TypedHook<ConcreteContext>
 * on a per-class basis.
 */
using NodeHook = TypedHook<>;

/**
 * @typedef NodeCondition
 * @brief Predicate function type for conditional callbacks.
 *
 * Evaluated each tick; the paired callback fires only when this returns true.
 *
 * Example:
 * ```cpp
 * node->on_tick_if(
 *     [](NodeContext& ctx) { return ctx.value > 0.8; },
 *     [](NodeContext& ctx) { ... }
 * );
 * ```
 */
using NodeCondition = std::function<bool(NodeContext&)>;

/**
 * @typedef HookId
 * @brief Identifies one registration inside a HookList. Zero is never issued.
 */
using HookId = uint64_t;

/**
 * @struct HookEntry
 * @brief One registration held by a HookList.
 *
 * key is the type of the callable the caller handed in, so removal by callable keeps
 * working when the stored hook is an adapter. tag and params are interpreted by the
 * list's owner (event kind, threshold, range, target).
 */
struct HookEntry {
    HookId id {};
    const std::type_info* key { &typeid(void) };
    uint8_t tag {};
    std::array<double, 2> params {};
    NodeHook hook;
    std::optional<NodeCondition> condition;
};

/**
 * @class HookList
 * @brief Stores, fires and clears the hooks of one node channel.
 *
 * A HookList registers itself with the Node that owns it, so Node::remove_all_hooks
 * reaches every list without any node class overriding it. Lists are not copyable.
 */
class MAYAFLUX_API HookList {
public:
    explicit HookList(Node& owner);

    ~HookList() = default;

    HookList(const HookList&) = delete;
    HookList& operator=(const HookList&) = delete;
    HookList(HookList&&) = delete;
    HookList& operator=(HookList&&) = delete;

    /** @brief Appends a hook with no duplicate check. */
    HookId add(NodeHook hook, const std::type_info& key, uint8_t tag = 0, std::array<double, 2> params = {});

    /** @brief Appends unless an entry with the same key and params exists. Returns 0 when dropped. */
    HookId add_unique(NodeHook hook, const std::type_info& key, uint8_t tag = 0, std::array<double, 2> params = {});

    /** @brief Appends a hook that fires only while condition holds. */
    HookId add_conditional(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag = 0);

    /** @brief As add_conditional, dropped when the same hook and condition types exist. Returns 0 when dropped. */
    HookId add_conditional_unique(NodeHook hook, NodeCondition condition, const std::type_info& key, uint8_t tag = 0);

    /** @brief Removes the registration with this id. */
    bool remove(HookId id);

    /** @brief Removes every entry registered with this callable type. */
    bool remove_by_key(const std::type_info& key);

    /** @brief Removes the first entry registered with this callable type. */
    bool remove_first_by_key(const std::type_info& key);

    /** @brief Removes every entry whose condition has this callable type. */
    bool remove_by_condition_key(const std::type_info& key);

    /** @brief Removes every entry carrying this tag. */
    bool remove_by_tag(uint8_t tag);

    void clear();

    [[nodiscard]] bool empty() const { return m_entries.empty(); }

    /** @brief Calls every entry whose condition, if any, holds. */
    void fire(NodeContext& ctx) const;

    [[nodiscard]] const std::vector<HookEntry>& entries() const { return m_entries; }

private:
    std::vector<HookEntry> m_entries;
    HookId m_next_id { 1 };
};

/**
 * @brief Wraps a context-typed hook into a NodeHook.
 *
 * The wrapper downcasts the context once, so callers receive their concrete context type.
 */
template <typename ContextT>
NodeHook adapt_hook(const TypedHook<ContextT>& hook)
{
    return [hook](NodeContext& ctx) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        hook(static_cast<ContextT&>(ctx));
    };
}

/**
 * @brief Atomically sets a node state flag with strong memory ordering
 * @param flag The atomic node state to modify
 * @param expected The expected current state value
 * @param desired The desired new state value
 *
 * This function safely updates a node's state, ensuring that state transitions
 * are consistent across the audio processing graph. Node states track important
 * conditions like whether a node is active, processed, or pending removal,
 * which are critical for coordinating audio signal flow.
 */
MAYAFLUX_API void atomic_set_strong(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired);

/**
 * @brief Atomically sets a node state flag to a specific value
 * @param flag The atomic node state to modify
 * @param desired The desired new state value
 *
 * Forcefully updates a node's state to a specific value. This is used when
 * the node needs to be placed into a definitive state regardless of its
 * current condition, such as when activating or deactivating nodes in the
 * audio processing chain.
 */
MAYAFLUX_API void atomic_set_flag_strong(std::atomic<NodeState>& flag, const NodeState& desired);

/**
 * @brief Atomically adds a flag to a node state
 * @param state The atomic node state to modify
 * @param flag The flag to add to the state
 *
 * Adds a specific state flag to a node's state without affecting other state flags.
 * This is used to mark nodes with specific conditions (like PROCESSED or ACTIVE)
 * while preserving other aspects of the node's current state.
 */
MAYAFLUX_API void atomic_add_flag(std::atomic<NodeState>& state, NodeState flag);

/**
 * @brief Atomically removes a flag from a node state
 * @param state The atomic node state to modify
 * @param flags The flags to remove from the state
 *
 * Removes specific state flags from a node's state. This is commonly used to
 * clear processing markers after a node has been processed, or to remove
 * special states like PENDING_REMOVAL when they're no longer applicable.
 */
MAYAFLUX_API void atomic_remove_flag(std::atomic<NodeState>& state, NodeState flags);

/**
 * @brief Atomically sets a node state flag with weak memory ordering
 * @param flag The atomic node state to modify
 * @param expected The expected current state value
 * @param desired The desired new state value
 *
 * A performance-optimized version of state setting that's used in less
 * critical paths of the audio engine. This helps maintain node state
 * consistency while potentially improving performance in high-throughput
 * audio processing scenarios.
 */
MAYAFLUX_API void atomic_set_flag_weak(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired);

/**
 * @brief Atomically increments the modulator count by a specified amount
 * @param count The atomic counter to increment
 * @param amount The amount to increment by
 *
 * Increases a node's modulator count, which tracks how many other nodes are
 * currently using this node as a modulation source. This count is crucial for
 * determining when a node's processed state can be safely reset, preventing
 * redundant processing while ensuring all dependent nodes receive the correct
 * modulation values.
 */
MAYAFLUX_API void atomic_inc_modulator_count(std::atomic<uint32_t>& count, int amount);

/**
 * @brief Atomically decrements the modulator count by a specified amount
 * @param count The atomic counter to decrement
 * @param amount The amount to decrement by
 *
 * Decreases a node's modulator count when it's no longer being used as a
 * modulation source by another node. When the count reaches zero, the node
 * becomes eligible for state resets, allowing the audio engine to optimize
 * processing and avoid redundant calculations in the signal chain.
 */
MAYAFLUX_API void atomic_dec_modulator_count(std::atomic<uint32_t>& count, int amount);

/**
 * @brief Attempts to reset the processed state of a node
 * @param node The node whose processed state should be reset
 *
 * Evaluates whether a node's processed state can be safely reset based on its
 * current modulator count and other conditions. This is essential for the audio
 * engine's processing cycle, as it determines which nodes need to be recalculated
 * in the next cycle and which can reuse their previous output values, balancing
 * processing efficiency with signal accuracy.
 */
MAYAFLUX_API void try_reset_processed_state(std::shared_ptr<Node> node);

/**
 * @brief Extracts active channel list from a node's channel mask
 * @param node Node to inspect (can be null)
 * @param fallback_channel Channel to use if node has no active channels (default: 0)
 * @return Vector of active channel indices
 */
MAYAFLUX_API std::vector<uint32_t> get_active_channels(const std::shared_ptr<Nodes::Node>& node, uint32_t fallback_channel = 0);

/**
 * @brief Extracts active channel list from a channel mask
 * @param channel_mask Bitmask of active channels
 * @param fallback_channel Channel to use if mask is 0 (default: 0)
 * @return Vector of active channel indices
 */
MAYAFLUX_API std::vector<uint32_t> get_active_channels(uint32_t channel_mask, uint32_t fallback_channel = 0);

/**
 * @brief Updates the routing state for a node based on its current channel usage
 * @param state The routing state to update
 *
 * This function evaluates the current channel usage of a node and updates the
 * routing state accordingly. It manages transitions between different routing
 * phases (such as fade-in and fade-out(Active)) based on changes in channel counts,
 * ensuring smooth audio output during dynamic reconfigurations of the processing graph.
 */
MAYAFLUX_API void update_routing_state(RoutingState& state);

}
