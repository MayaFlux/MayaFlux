#pragma once

#include "NodeSpec.hpp"

namespace MayaFlux::Nodes {

class Node;
class NodeContext;

/**
 * @typedef NodeHook
 * @brief Callback function type for node processing events
 *
 * A NodeHook is a function that receives a NodeContext object containing
 * information about the node's current state. These callbacks are triggered
 * during node processing to notify external components about node activity.
 *
 * Example:
 * ```cpp
 * node->on_tick([](NodeContext& ctx) {
 *     std::cout << "Node produced value: " << ctx.value << std::endl;
 * });
 * ```
 */
using NodeHook = std::function<void(NodeContext&)>;

/**
 * @typedef NodeCondition
 * @brief Predicate function type for conditional callbacks
 *
 * A NodeCondition is a function that evaluates whether a callback should
 * be triggered based on the node's current state. It receives a NodeContext
 * object and returns true if the condition is met, false otherwise.
 *
 * Example:
 * ```cpp
 * node->on_tick_if(
 *     [](NodeContext& ctx) { return ctx.value > 0.8; },
 *     [](NodeContext& ctx) { std::cout << "Threshold exceeded!" << std::endl; }
 * );
 * ```
 */
using NodeCondition = std::function<bool(NodeContext&)>;

/**
 * @brief Checks if a callback function already exists in a collection
 * @param callbacks The collection of callback functions to search
 * @param callback The callback function to look for
 * @return True if the callback exists in the collection, false otherwise
 *
 * This function compares function pointers to determine if a specific callback
 * is already registered in a collection. It's used to prevent duplicate
 * registrations of the same callback function.
 */
bool callback_exists(const std::vector<NodeHook>& callbacks, const NodeHook& callback);

/**
 * @brief Checks if a condition function already exists in a collection of conditional callbacks
 * @param callbacks The collection of conditional callbacks to search
 * @param callback The condition function to look for
 * @return True if the condition exists in the collection, false otherwise
 *
 * This function compares function pointers to determine if a specific condition
 * is already used in a collection of conditional callbacks. It's used to prevent
 * duplicate registrations of the same condition function.
 */
bool conditional_callback_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeCondition& callback);

/**
 * @brief Checks if a specific callback and condition pair already exists
 * @param callbacks The collection of conditional callbacks to search
 * @param callback The callback function to look for
 * @param condition The condition function to look for
 * @return True if the exact pair exists in the collection, false otherwise
 *
 * This function checks if a specific combination of callback and condition
 * functions is already registered. It's used to prevent duplicate registrations
 * of the same callback-condition pair.
 */
bool callback_pair_exists(const std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeHook& callback, const NodeCondition& condition);

/**
 * @brief Safely adds a callback to a collection if it doesn't already exist
 * @param callbacks The collection of callbacks to add to
 * @param callback The callback function to add
 * @return True if the callback was added, false if it already existed
 *
 * This function first checks if the callback already exists in the collection,
 * and only adds it if it's not already present. This prevents duplicate
 * registrations of the same callback function.
 */
bool safe_add_callback(std::vector<NodeHook>& callbacks, const NodeHook& callback);

/**
 * @brief Safely adds a conditional callback if it doesn't already exist
 * @param callbacks The collection of conditional callbacks to add to
 * @param callback The callback function to add
 * @param condition The condition function to add
 * @return True if the conditional callback was added, false if it already existed
 *
 * This function first checks if the exact callback-condition pair already exists
 * in the collection, and only adds it if it's not already present. This prevents
 * duplicate registrations of the same conditional callback.
 */
bool safe_add_conditional_callback(std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeHook& callback, const NodeCondition& condition);

/**
 * @brief Safely removes a callback from a collection
 * @param callbacks The collection of callbacks to remove from
 * @param callback The callback function to remove
 * @return True if the callback was found and removed, false otherwise
 *
 * This function searches for the specified callback in the collection and
 * removes it if found. It's used to unregister callbacks when they're no
 * longer needed.
 */
bool safe_remove_callback(std::vector<NodeHook>& callbacks, const NodeHook& callback);

/**
 * @brief Safely removes all conditional callbacks with a specific condition
 * @param callbacks The collection of conditional callbacks to remove from
 * @param callback The condition function to remove
 * @return True if at least one conditional callback was found and removed, false otherwise
 *
 * This function searches for all conditional callbacks that use the specified
 * condition function and removes them. It's used to unregister conditional
 * callbacks when they're no longer needed.
 */
bool safe_remove_conditional_callback(std::vector<std::pair<NodeHook, NodeCondition>>& callbacks, const NodeCondition& callback);

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

}
