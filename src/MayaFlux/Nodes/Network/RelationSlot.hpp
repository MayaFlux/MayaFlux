#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @struct RelationSlot
 * @brief One addressable participant of a RelationNetwork
 *
 * A plain record: an optional node, a level, an activity flag, and the last
 * rendered block. Fields are public and operators read them directly; the
 * few methods below exist only where a field needs more than a plain
 * assignment (firing callbacks, enabling a node's hooks).
 *
 * @warning A slot's address is not stable: RelationNetwork::add_slot() may
 *          reallocate the whole slot vector. Never hold a pointer or
 *          reference to a RelationSlot past a process_batch() call; read a
 *          slot fresh through RelationNetwork::get_slot(index) or
 *          get_node_output(index) instead.
 *
 * Structural edits (assigning a node, registering callbacks) must happen
 * outside process_batch().
 *
 * A plain aggregate: RelationNetwork::add_slot() takes one by value, so it
 * can be built with a designated initializer instead of a set_node() call
 * afterward.
 *
 * @code
 * n->add_slot({ .name = "carrier", .node = vega.Sine(220.0, 1.0) });
 * n->add_slot({ .name = "product" }); // no node
 * @endcode
 *
 * @note .node set this way still gets its hooks enabled: add_slot()
 *       applies that regardless of whether the node arrived through
 *       set_node() or through the .node field directly.
 */
struct RelationSlot {
    uint32_t index {};
    std::string name;
    std::shared_ptr<Node> node;
    double level { 0.0 };
    bool active { true };
    std::vector<double> block;

    std::vector<std::function<void()>> on_enter;
    std::vector<std::function<void()>> on_leave;

    /**
     * @brief Set this slot's node
     * @param new_node Node whose output becomes this slot's signal. Owned by the slot.
     *
     * Enables the node's hooks (on_impulse, on_phase_wrap, on_tick, ...) so
     * they fire while the slot is advanced. A node already belonging to
     * another network stays silent until set_in_network(false) is called on
     * it directly.
     */
    void set_node(std::shared_ptr<Node> new_node)
    {
        if (new_node) {
            new_node->m_fire_events_during_snapshot = true;
        }
        node = std::move(new_node);
    }

    /**
     * @brief Set this slot's activity, firing on_enter/on_leave on a real transition
     */
    void set_active(bool is_active)
    {
        if (active == is_active) {
            return;
        }
        active = is_active;
        for (const auto& callback : is_active ? on_enter : on_leave) {
            if (callback) {
                callback();
            }
        }
    }

    /**
     * @brief Preallocate this slot's block storage without changing its size
     */
    void reserve_block(uint32_t block_frames)
    {
        block.reserve(block_frames);
    }

    /**
     * @brief Size this slot's block for writing
     * @return Writable block, zero filled. Does not allocate within reserved capacity.
     */
    std::span<double> prepare_block(uint32_t frames)
    {
        block.assign(frames, 0.0);
        return block;
    }
};

} // namespace MayaFlux::Nodes::Network
