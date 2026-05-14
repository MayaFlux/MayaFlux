#pragma once

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Portal/Forma/Link.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Collapsible.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Result of an introspect call.
 *
 * Owns all Forma constructs produced for one subject. Children are nested
 * InspectResults for sub-objects. The caller's only per-tick obligation is
 * tap_all() from a GraphicsRoutine.
 */
struct InspectResult {
    Collapsible collapsible;
    std::shared_ptr<Core::VKImage> text;
    std::vector<Link> links;
    std::vector<InspectResult> children;

    /**
     * @brief Tap all links in this result and all children, recursively.
     * Call once per graphics tick.
     */
    void tap_all()
    {
        for (auto& l : links)
            l.tap();
        for (auto& c : children)
            c.tap_all();
    }

    InspectResult() = default;
    InspectResult(InspectResult&&) noexcept = default;
    InspectResult& operator=(InspectResult&&) noexcept = default;
    InspectResult(const InspectResult&) = delete;
    InspectResult& operator=(const InspectResult&) = delete;
};

} // namespace MayaFlux::Portal::Forma
