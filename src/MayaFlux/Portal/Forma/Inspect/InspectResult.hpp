#pragma once

#include "QueryUtils.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Result of an introspect call.
 *
 * Owns the ValueGroup (header collapsible + value rows) and nested
 * InspectResults for sub-objects. The caller's only per-tick obligation
 * is tap_all() from a GraphicsRoutine.
 */
struct InspectResult {
    ValueGroup group;
    std::vector<InspectResult> children;

    /**
     * @brief Tap all row links in this result and all children, recursively.
     * Call once per graphics tick.
     */
    void tap_all()
    {
        for (auto& row : group.rows)
            row.link.tap();
        for (auto& c : children)
            c.tap_all();
    }

    InspectResult() = default;
    ~InspectResult() = default;

    InspectResult(InspectResult&&) noexcept = default;
    InspectResult& operator=(InspectResult&&) noexcept = default;
    InspectResult(const InspectResult&) = delete;
    InspectResult& operator=(const InspectResult&) = delete;
};

} // namespace MayaFlux::Portal::Forma
