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

    /**
     * @brief Inspect a NodeNetwork and its per-network metadata.
     *
     * Builds a collapsible ValueGroup whose header is the network's topology
     * and output mode, with body rows for node count and enabled state.
     * Intended to be nested inside node_graph_manager() but callable
     * independently when the caller already holds a network reference.
     */
    [[nodiscard]] InspectResult node_network(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& net,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect the full NodeGraphManager state across all active tokens.
     *
     * Emits one collapsible per active Nodes::ProcessingToken. Each token
     * expands per-channel children, each of which recursively calls node()
     * for every node registered to that channel. Networks for the token are
     * appended after the channel children, showing topology, output mode,
     * node count, enabled state, and registered channels.
     */
    [[nodiscard]] InspectResult node_graph_manager(
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

    /**
     * @brief Inspect a single Buffer: default processor and full processing chain.
     *
     * Header is the buffer's dynamic type name. Body rows expose the default
     * processor type, then preprocessor, each indexed chain processor,
     * postprocessor, and final processor. Absent slots show "-".
     */
    [[nodiscard]] InspectResult buffer(
        const std::shared_ptr<Buffers::Buffer>& buf,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect the root audio buffer for a specific token and channel.
     *
     * Header shows channel index and token. Rows show sample count and child
     * count. The root buffer itself is expanded via buffer(), then each child
     * buffer is expanded via buffer() in order.
     */
    [[nodiscard]] InspectResult root_audio_buffer(
        Buffers::ProcessingToken token, uint32_t channel,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect all root audio buffers for a token across every channel.
     *
     * Header shows token and total channel count. Each channel is expanded
     * as a nested root_audio_buffer() result, related for visibility cascade.
     */
    [[nodiscard]] InspectResult root_audio_buffer(
        Buffers::ProcessingToken token,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect the root graphics buffer for a token.
     *
     * Header shows token. Row shows VKBuffer child count. The root buffer
     * itself is expanded via buffer(), then each VKBuffer child via buffer().
     */
    [[nodiscard]] InspectResult root_graphics_buffer(
        Buffers::ProcessingToken token,
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect the full BufferManager state across all active tokens.
     *
     * Emits one top-level collapsible per active Buffers::ProcessingToken. Audio
     * tokens (AUDIO_BACKEND, AUDIO_PARALLEL) expand per-channel rows showing
     * sample count, child buffer count, and processing chain state. Graphics
     * tokens (GRAPHICS_BACKEND) expand a single root entry showing VKBuffer
     * child count. All children are related via layer.relate() for cascade.
     */
    [[nodiscard]] InspectResult buffer_manager(
        Layer& layer, Context& context,
        const std::shared_ptr<Core::Window>& window,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

private:
    Buffers::BufferManager& m_bm;
    Nodes::NodeGraphManager& m_ngm;
};

} // namespace MayaFlux::Portal::Forma
