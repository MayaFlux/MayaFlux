#pragma once

#include "InspectResult.hpp"
#include "MayaFlux/Portal/Forma/Primitives/LayoutCursor.hpp"

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Nodes {
struct ModulatorTree;
class NodeGraphManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
struct TaskEntry;
class Event;
class EventManager;
}

namespace MayaFlux::Portal::Forma {

class Surface;

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
    Inspector(Nodes::NodeGraphManager& ngm, Buffers::BufferManager& bm,
        Vruta::TaskScheduler& sched, Vruta::EventManager& event_mgr)
        : m_ngm(ngm)
        , m_bm(bm)
        , m_sched(sched)
        , m_event_mgr(event_mgr)
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
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect a single RootNode for a specific token and channel.
     *
     * Header is "root [ch N]" for AUDIO_RATE or "root" for all other tokens.
     * Body row exposes live node count from RootNode::get_node_size(). Each
     * registered node is expanded via node() as a child. Networks for the
     * token/channel are appended after the node children.
     */
    [[nodiscard]] InspectResult root_node(
        Nodes::ProcessingToken token, uint32_t channel,
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F, int depth = 0);

    /**
     * @brief Inspect all RootNodes for a token.
     *
     * For AUDIO_RATE: header shows token and channel count, each channel
     * expanded as a nested root_node(token, channel) result.
     * For all other tokens: delegates directly to root_node(token, 0).
     */
    [[nodiscard]] InspectResult root_node(
        Nodes::ProcessingToken token,
        Surface& surface,
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
        Surface& surface,
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
     *
     * Persistent: the first call builds the result against @p surface and
     * @p cursor and stores it statically. Subsequent calls return the same
     * result regardless of the supplied surface and cursor; the original
     * window remains the render target. To display on a different surface,
     * the process must be restarted.
     */
    [[nodiscard]] InspectResult& node_graph_manager(
        Surface& surface,
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
        Surface& surface,
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
        Surface& surface,
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
        Surface& surface,
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
        Surface& surface,
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
     *
     * Persistent: the first call builds the result against @p surface and
     * @p cursor and stores it statically. Subsequent calls return the same
     * result regardless of the supplied surface and cursor; the original
     * window remains the render target. To display on a different surface,
     * the process must be restarted.
     */
    [[nodiscard]] InspectResult& buffer_manager(
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

    /**
     * @brief Inspect the full TaskScheduler state.
     *
     * Header shows total task count. Each task is a collapsible whose header
     * is the routine's dynamic type name, with name, token, active, and delay
     * context as sub-fields.
     *
     * Persistent: the first call builds the result against @p surface and
     * @p cursor and stores it statically. Subsequent calls return the same
     * result regardless of the supplied surface and cursor; the original
     * window remains the render target. To display on a different surface,
     * the process must be restarted.
     */
    [[nodiscard]] InspectResult& scheduler(
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

    /**
     * @brief Inspect tasks for a specific processing token.
     *
     * Same per-task layout as scheduler() but filtered to one token.
     */
    [[nodiscard]] InspectResult tasks(
        Vruta::ProcessingToken token,
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);
    /**
     * @brief Inspect the full EventManager state.
     *
     * Header shows total event count. Each event is a collapsible whose
     * header is the event's registered name, or "(unnamed)" for events
     * added without a name. Body rows expose active state.
     *
     * Persistent: the first call builds the result against @p surface and
     * @p cursor and stores it statically. Subsequent calls return the same
     * result regardless of the supplied surface and cursor; the original
     * window remains the render target. To display on a different surface,
     * the process must be restarted.
     */
    [[nodiscard]] InspectResult& event_manager(
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

    /**
     * @brief Inspect a single Event by name and pointer.
     *
     * Header is the event's registered name, or "(unnamed)" if empty.
     * Body rows expose token and active state.
     */
    [[nodiscard]] InspectResult event(
        const std::shared_ptr<Vruta::Event>& ev,
        std::string_view name,
        Surface& surface,
        LayoutCursor& cursor,
        float x_min = -0.95F, float x_max = 0.95F, float row_h = 0.05F);

private:
    Buffers::BufferManager& m_bm;
    Nodes::NodeGraphManager& m_ngm;
    Vruta::TaskScheduler& m_sched;
    Vruta::EventManager& m_event_mgr;

    [[nodiscard]] RowBuffer make_row_buffer(
        const std::shared_ptr<Core::Window>& window,
        std::string_view text,
        glm::uvec2 pixel_dims) const;

    InspectResult inspect_modulator_tree(
        const Nodes::ModulatorTree& tree,
        Surface& surface,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h, int depth);

    [[nodiscard]] InspectResult inspect_task(
        const Vruta::TaskEntry& entry,
        Surface& surface,
        LayoutCursor& cursor,
        float x_min, float x_max, float row_h);

    static std::optional<InspectResult> s_node_graph_result;
    static std::optional<InspectResult> s_buffer_result;
    static std::optional<InspectResult> s_scheduler_result;
    static std::optional<InspectResult> s_event_result;
};

} // namespace MayaFlux::Portal::Forma
