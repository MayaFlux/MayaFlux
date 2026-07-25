#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Portal/Graphics/ShaderSpec.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Buffers {

class RelaxationStepProcessor;
class RelaxationEmitProcessor;
class RenderProcessor;

/**
 * @class RelaxationGridBuffer
 * @brief GPU-resident, double-buffered SSBO state for synchronous
 *        local-rule systems evaluated over a fixed 2D topology: cellular
 *        automata, Jacobi/Gauss-Seidel relaxation, discrete diffusion, and
 *        any rule where every cell updates from its own and neighbors'
 *        previous-generation state.
 *
 * Grid state lives entirely as two raw Vulkan handle pairs inside
 * VKBufferResources::back_buffers, allocated once at construction. No
 * VKBuffer object represents the grid state at any point; descriptor
 * writes for these handles are issued directly against ShaderFoundry by
 * RelaxationStepProcessor and RelaxationEmitProcessor, bypassing
 * ShaderProcessor::bind_buffer entirely, since that call requires a
 * VKBuffer wrapper this design does not create.
 *
 * The VKBuffer base itself (this object) owns only the emitted vertex
 * output (Usage::VERTEX), written each generation by
 * RelaxationEmitProcessor from the front generation's state, then drawn
 * directly by RenderProcessor as ordinary untextured point geometry.
 *
 * Chain order:
 *   default  — RelaxationStepProcessor (rule shader: state front -> back)
 *   flat[0]  — RelaxationEmitProcessor (emit shader: front state -> vertices)
 *   final    — RenderProcessor
 *
 * Snapshot delivery uses a BroadcastSource<std::vector<uint8_t>>, owned by
 * this buffer and exposed via snapshot_source(). RelaxationStepProcessor
 * signals it directly once a requested download completes; there is no
 * intermediate queue or poll. Subscribe via Kriya::on_signal() or
 * on_signal_matching() at whatever cadence the caller chooses — the class
 * imposes none.
 *
 * Usage:
 * @code
 * auto grid = std::make_shared<RelaxationGridBuffer>(
 *     256, 256, sizeof(uint32_t), "ca_rule.comp", "ca_emit.comp");
 * grid->setup_processors(ProcessingToken::GRAPHICS_BACKEND);
 * grid->setup_rendering({ .target_window = window });
 * @endcode
 */
class MAYAFLUX_API RelaxationGridBuffer : public VKBuffer {
public:
    /**
     * @brief Either a shader file path or a generated ShaderSpec, resolved at
     *        construction time by setup_processors().
     */
    using ShaderSource = std::variant<std::string, Portal::Graphics::ShaderSpec>;

    /**
     * @brief Construct an unregistered double-buffered grid buffer.
     * @param width Grid width in cells.
     * @param height Grid height in cells.
     * @param cell_stride_bytes Size in bytes of one cell's state.
     * @param rule_source Either a path to a hand-written rule shader (for
     *        rules needing branchy multi-state logic, e.g. Conway, Wireworld)
     *        or a generated ShaderSpec (e.g. RelaxationSpecs::jacobi_diffusion()).
     * @param emit_source Either a path to a hand-written emit shader, or a
     *        generated ShaderSpec (e.g. RelaxationSpecs::emit_binary()).
     *
     * rule_source and emit_source are independent: a hand-written rule paired
     * with a generated emit spec, or any other combination, is fully
     * supported. Processors are constructed from whichever alternative each
     * holds in setup_processors().
     */
    RelaxationGridBuffer(
        uint32_t width,
        uint32_t height,
        size_t cell_stride_bytes,
        ShaderSource rule_source,
        ShaderSource emit_source);

    /**
     * @brief Destructor.
     *
     * Raw handles in m_resources.back_buffers are released by the backend
     * during buffer service teardown, consistent with how VKBuffer's own
     * primary buffer/memory are released; this class performs no manual
     * Vulkan destruction itself.
     */
    ~RelaxationGridBuffer() override = default;

    /**
     * @brief Create and attach RelaxationStepProcessor (default processor)
     *        and RelaxationEmitProcessor (flat processor), and set the
     *        per-cell vertex layout on this buffer's own storage.
     * @param token Processing domain (typically GRAPHICS_BACKEND).
     */
    void setup_processors(ProcessingToken token) override;

    /**
     * @brief Attach a RenderProcessor drawing the emitted vertex output as
     *        ordinary untextured point geometry.
     * @param config Render target; vertex/fragment shaders default to the
     *               standard untextured pair unless overridden. Topology
     *               is always forced to POINT_LIST regardless of what is
     *               supplied in @p config.
     */
    void setup_rendering(const RenderConfig& config);

    /** @brief Grid width in cells. */
    [[nodiscard]] uint32_t get_grid_width() const { return m_width; }

    /** @brief Grid height in cells. */
    [[nodiscard]] uint32_t get_grid_height() const { return m_height; }

    /** @brief Total cell count, width * height. */
    [[nodiscard]] uint32_t get_cell_count() const { return m_width * m_height; }

    /** @brief Total byte size of one generation's state buffer. */
    [[nodiscard]] size_t get_state_bytes() const
    {
        return static_cast<size_t>(m_width) * m_height * m_cell_stride_bytes;
    }

    /**
     * @brief Write initial state directly into the current front generation.
     * @param data Pointer to cell_count * cell_stride_bytes of state data.
     * @param size Byte count; must equal get_state_bytes().
     *
     * Direct host-visible memcpy into back_buffers[front_index()], mirroring
     * VKBuffer::set_data's own host-visible write path. Valid only before the
     * buffer's processing chain has run at least once, or between generations
     * where overwriting the current front state is intended (e.g. a runtime
     * reset). Does not touch the back generation or trigger a swap.
     */
    void seed_state(const void* data, size_t size);

    /**
     * @brief Index into m_resources.back_buffers currently holding the
     *        most recently completed generation's state.
     */
    [[nodiscard]] uint32_t front_index() const { return m_front_is_a ? 0 : 1; }

    /** @brief Temporary probe: total swap_generation calls since construction. */
    [[nodiscard]] uint32_t swap_count() const { return m_swap_count; }

    /**
     * @brief Index into m_resources.back_buffers currently serving as the
     *        write target for the next rule dispatch.
     */
    [[nodiscard]] uint32_t back_index() const { return m_front_is_a ? 1 : 0; }

    /**
     * @brief Request a snapshot of the next completed generation.
     *
     * Wait-free. Consumed by RelaxationStepProcessor on the next
     * on_after_execute call, which stages a device-to-host download of the
     * newly-front state buffer and signals snapshot_source() with the result.
     */
    void request_snapshot() { m_snapshot_requested.store(true, std::memory_order_release); }

    /**
     * @brief BroadcastSource of completed generation snapshots.
     *
     * Signaled by RelaxationStepProcessor exactly once per fulfilled
     * request_snapshot() call, at the point the device-to-host download
     * actually completes — not polled, not tied to any particular pacing.
     * Subscribe via Kriya::on_signal()/on_signal_matching() at whatever
     * cadence suits the caller; the class itself imposes none.
     */
    [[nodiscard]] std::shared_ptr<Vruta::BroadcastSource<std::vector<uint8_t>>> snapshot_source() const
    {
        return m_snapshot_source;
    }

private:
    friend class RelaxationStepProcessor;
    friend class RelaxationEmitProcessor;

    [[nodiscard]] bool consume_snapshot_request()
    {
        return m_snapshot_requested.exchange(false, std::memory_order_acq_rel);
    }

    void swap_generation()
    {
        m_front_is_a = !m_front_is_a;
        ++m_swap_count;
    }

    /** @brief Signaled by RelaxationStepProcessor when a requested snapshot completes. */
    std::shared_ptr<Vruta::BroadcastSource<std::vector<uint8_t>>> m_snapshot_source {
        std::make_shared<Vruta::BroadcastSource<std::vector<uint8_t>>>()
    };

    uint32_t m_width; ///< Grid width in cells.
    uint32_t m_height; ///< Grid height in cells.
    size_t m_cell_stride_bytes; ///< Size in bytes of one cell's state, fixed at construction.

    ShaderSource m_rule_source; ///< Either a path to a hand-written rule shader or a generated ShaderSpec.
    ShaderSource m_emit_source; ///< Either a path to a hand-written emit shader or a generated ShaderSpec.

    bool m_front_is_a { true }; ///< True when back_buffers[0] holds the current front generation.
    std::atomic<bool> m_snapshot_requested {}; ///< Set by request_snapshot(), cleared by consume_snapshot_request().
    uint32_t m_swap_count {}; ///< Temporary probe: total swap_generation calls.
};

} // namespace MayaFlux::Buffers
