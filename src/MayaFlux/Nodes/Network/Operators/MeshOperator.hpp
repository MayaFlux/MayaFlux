#pragma once

#include "MayaFlux/Nodes/Network/MeshSlot.hpp"
#include "NetworkOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class MeshOperator
 * @brief Abstract base for operators that process MeshNetwork slots.
 *
 * Sits between NetworkOperator and the concrete mesh operators.
 * Concrete subclasses implement process_slot() for per-slot logic;
 * the base process() iterates m_slots in the caller-supplied topological
 * order and delegates to process_slot() for each entry.
 *
 * Slots are supplied once per cycle by MeshNetwork::process_batch()
 * via set_slots(). The operator does not own the slot storage.
 *
 * Topological order (parents before children) is enforced by MeshNetwork
 * before it calls process(); operators receive slots in that order and
 * must not re-order them.
 */
class MAYAFLUX_API MeshOperator : public NetworkOperator {
public:
    ~MeshOperator() override = default;

    /**
     * @brief Supply the slot list and processing order for the coming cycle.
     * @param slots   Reference to the MeshNetwork's slot storage.
     * @param order   Indices into slots in topological order (parents first).
     *
     * Called by MeshNetwork::process_batch() before process(dt).
     * The reference must remain valid for the duration of the cycle.
     */
    void set_slots(std::vector<MeshSlot>& slots,
        const std::vector<uint32_t>& order)
    {
        m_slots = &slots;
        m_order = &order;
    }

    /**
     * @brief Iterate slots in topological order and call process_slot() on each.
     * @param dt Time delta in seconds.
     */
    void process(float dt) override
    {
        if (!m_slots || !m_order)
            return;
        for (uint32_t idx : *m_order)
            process_slot((*m_slots)[idx], dt);
    }

    /**
     * @brief Process a single slot.
     * @param slot Slot to process (mutable).
     * @param dt   Time delta in seconds.
     */
    virtual void process_slot(MeshSlot& slot, float dt) = 0;

    // -------------------------------------------------------------------------
    // NetworkOperator stubs -- concrete operators may override as needed
    // -------------------------------------------------------------------------

    void set_parameter(std::string_view, double) override { }
    [[nodiscard]] std::optional<double> query_state(std::string_view) const override
    {
        return std::nullopt;
    }

protected:
    std::vector<MeshSlot>* m_slots { nullptr };
    const std::vector<uint32_t>* m_order { nullptr };
};

} // namespace MayaFlux::Nodes::Network
