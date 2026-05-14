#pragma once

#include "InspectResult.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Portal/Forma/Primitives/LayoutCursor.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Forma::Inspect {

/**
 * @brief Inspect a single Node and its full modulator tree.
 */
[[nodiscard]] InspectResult node(
    Buffers::BufferManager& bm,
    const std::shared_ptr<Nodes::Node>& n,
    Layer& layer,
    Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min = -0.95F,
    float x_max = 0.95F,
    float row_h = 0.05F,
    int depth = 0);

} // namespace MayaFlux::Portal::Forma::Inspect
