#pragma once

#include "MayaFlux/Nodes/Network/GeometrySlot.hpp"
#include "NetworkOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class InstanceOperator
 * @brief Abstract base for operators that process InstanceNetwork slots.
 *
 * Mirrors MeshOperator but operates over a flat GeometrySlot vector with
 * no topological ordering. Subclasses implement process_slot().
 */
class MAYAFLUX_API InstanceOperator : public NetworkOperator {
public:
    ~InstanceOperator() override = default;

    void set_slots(std::vector<GeometrySlot>& slots) { m_slots = &slots; }

    void process(float dt) override
    {
        if (!m_slots)
            return;

        for (auto& slot : *m_slots)
            process_slot(slot, dt);
    }

    virtual void process_slot(GeometrySlot& slot, float dt) = 0;

    void set_parameter(std::string_view, double) override { }

    [[nodiscard]] std::optional<double> query_state(std::string_view) const override
    {
        return std::nullopt;
    }

protected:
    std::vector<GeometrySlot>* m_slots { nullptr };
};

} // namespace MayaFlux::Nodes::Network
