#pragma once

#include "MayaFlux/Utils.hpp"

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
 * node->on_tick([](const NodeContext& ctx) {
 *     std::cout << "Node produced value: " << ctx.value << std::endl;
 * });
 * ```
 */
using NodeHook = std::function<void(const NodeContext&)>;

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
 *     [](const NodeContext& ctx) { std::cout << "Threshold exceeded!" << std::endl; },
 *     [](const NodeContext& ctx) { return ctx.value > 0.8; }
 * );
 * ```
 */
using NodeCondition = std::function<bool(const NodeContext&)>;

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

void atomic_set_strong(std::atomic<Utils::NodeState>& flag, Utils::NodeState& expected, const Utils::NodeState& desired);

void atomic_set_flag_strong(std::atomic<Utils::NodeState>& flag, const Utils::NodeState& desired);

void atomic_add_flag(std::atomic<Utils::NodeState>& state, Utils::NodeState flag);

void atomic_remove_flag(std::atomic<Utils::NodeState>& state, Utils::NodeState flags);

void atomic_set_flag_weak(std::atomic<Utils::NodeState>& flag, Utils::NodeState& expected, const Utils::NodeState& desired);
}
