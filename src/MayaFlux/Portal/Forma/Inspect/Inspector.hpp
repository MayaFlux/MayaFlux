#pragma once

#include "InspectResult.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"
#include "MayaFlux/Portal/Forma/Primitives/LayoutCursor.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief Forma subsystem for live introspection.
 *
 * Initialized by Portal::Forma::initialize alongside Bridge.
 * Provides a fluent entry point into Inspect:: query functions
 * using stored BufferManager and NodeGraphManager defaults.
 *
 * Access via Portal::Forma::get_inspector().
 */
class MAYAFLUX_API Inspector {
public:
    Inspector(Buffers::BufferManager& bm, Nodes::NodeGraphManager& ngm)
        : m_bm(bm)
        , m_ngm(ngm)
    {
    }

    /**
     * @brief Inspect a single Node and its full modulator tree.
     *
     * Builds a ValueGroup whose header is the node's short type name and
     * whose body rows expose the node's runtime values (output, role for
     * modulators, etc.). Modulator children are recursively expanded as
     * nested InspectResults related to this node's header.
     */
    [[nodiscard]] InspectResult node(
        const std::shared_ptr<Nodes::Node>& n,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

private:
    Buffers::BufferManager& m_bm;
    Nodes::NodeGraphManager& m_ngm;
};

} // namespace MayaFlux::Portal::Forma
