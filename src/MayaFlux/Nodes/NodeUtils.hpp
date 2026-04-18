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
 * @brief Returns true if an equivalent callback is already present in the collection.
 *
 * Equivalence is determined by target_type() -- the same limitation as std::function
 * comparison everywhere in the codebase.
 */
template <typename ContextT>
bool callback_exists(const std::vector<TypedHook<ContextT>>& callbacks,
    const TypedHook<ContextT>& callback)
{
    return std::ranges::any_of(callbacks,
        [&callback](const TypedHook<ContextT>& hook) {
            return hook.target_type() == callback.target_type();
        });
}

/**
 * @brief Adds a callback to the collection if an equivalent one is not already present.
 * @return True if added, false if a duplicate was detected.
 */
template <typename ContextT>
bool safe_add_callback(std::vector<TypedHook<ContextT>>& callbacks,
    const TypedHook<ContextT>& callback)
{
    if (!callback_exists(callbacks, callback)) {
        callbacks.push_back(callback);
        return true;
    }
    return false;
}

/**
 * @brief Removes all callbacks whose target_type() matches that of the supplied callback.
 * @return True if at least one entry was removed.
 */
template <typename ContextT>
bool safe_remove_callback(std::vector<TypedHook<ContextT>>& callbacks,
    const TypedHook<ContextT>& callback)
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

/**
 * @brief Returns true if a condition function is already present in a conditional callback collection.
 */
bool conditional_callback_exists(
    const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks,
    const NodeCondition& callback);

/**
 * @brief Returns true if the exact callback+condition pair is already present.
 */
bool callback_pair_exists(
    const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks,
    const NodeHook& callback,
    const NodeCondition& condition);

/**
 * @brief Adds a conditional callback if the exact pair is not already present.
 * @return True if added, false if a duplicate was detected.
 */
bool safe_add_conditional_callback(
    std::vector<std::pair<NodeHook, NodeCondition>>& callbacks,
    const NodeHook& callback,
    const NodeCondition& condition);

/**
 * @brief Removes all conditional callbacks whose condition target_type() matches.
 * @return True if at least one entry was removed.
 */
bool safe_remove_conditional_callback(
    std::vector<std::pair<NodeHook, NodeCondition>>& callbacks,
    const NodeCondition& callback);

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
void atomic_set_strong(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired);

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
void atomic_set_flag_strong(std::atomic<NodeState>& flag, const NodeState& desired);

/**
 * @brief Atomically adds a flag to a node state
 * @param state The atomic node state to modify
 * @param flag The flag to add to the state
 *
 * Adds a specific state flag to a node's state without affecting other state flags.
 * This is used to mark nodes with specific conditions (like PROCESSED or ACTIVE)
 * while preserving other aspects of the node's current state.
 */
void atomic_add_flag(std::atomic<NodeState>& state, NodeState flag);

/**
 * @brief Atomically removes a flag from a node state
 * @param state The atomic node state to modify
 * @param flags The flags to remove from the state
 *
 * Removes specific state flags from a node's state. This is commonly used to
 * clear processing markers after a node has been processed, or to remove
 * special states like PENDING_REMOVAL when they're no longer applicable.
 */
void atomic_remove_flag(std::atomic<NodeState>& state, NodeState flags);

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
void atomic_set_flag_weak(std::atomic<NodeState>& flag, NodeState& expected, const NodeState& desired);

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
void atomic_inc_modulator_count(std::atomic<uint32_t>& count, int amount);

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
void atomic_dec_modulator_count(std::atomic<uint32_t>& count, int amount);

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
void try_reset_processed_state(std::shared_ptr<Node> node);

/**
 * @brief Extracts active channel list from a node's channel mask
 * @param node Node to inspect (can be null)
 * @param fallback_channel Channel to use if node has no active channels (default: 0)
 * @return Vector of active channel indices
 */
std::vector<uint32_t> get_active_channels(const std::shared_ptr<Nodes::Node>& node, uint32_t fallback_channel = 0);

/**
 * @brief Extracts active channel list from a channel mask
 * @param channel_mask Bitmask of active channels
 * @param fallback_channel Channel to use if mask is 0 (default: 0)
 * @return Vector of active channel indices
 */
std::vector<uint32_t> get_active_channels(uint32_t channel_mask, uint32_t fallback_channel = 0);

/**
 * @brief Updates the routing state for a node based on its current channel usage
 * @param state The routing state to update
 *
 * This function evaluates the current channel usage of a node and updates the
 * routing state accordingly. It manages transitions between different routing
 * phases (such as fade-in and fade-out(Active)) based on changes in channel counts,
 * ensuring smooth audio output during dynamic reconfigurations of the processing graph.
 */
void update_routing_state(RoutingState& state);

}
