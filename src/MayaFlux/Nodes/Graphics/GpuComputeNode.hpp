#pragma once

#include "GpuSync.hpp"

#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GpuComputeContext
 * @brief Context delivered to on_complete callbacks when a dispatch completes.
 *
 * Carries the primary float readback and the aux map from GpuChannelResult.
 * The contents are valid only for the duration of the callback invocation;
 * callers that need to retain data must copy out of primary or aux.
 */
class MAYAFLUX_API GpuComputeContext : public NodeContext {
public:
    explicit GpuComputeContext(const Yantra::GpuChannelResult& result)
        : NodeContext(result.primary.empty() ? 0.0 : static_cast<double>(result.primary[0]))
        , gpu_result(result)
    {
    }

    /// @brief Reference to the full dispatch result. Valid during callback only.
    const Yantra::GpuChannelResult& gpu_result;
};

/**
 * @class GpuComputeNode
 * @brief GpuSync node that owns a ShaderExecutionContext and dispatches it
 *        asynchronously, polling for completion on each subsequent compute_frame().
 *
 * Lifecycle per dispatch cycle:
 *   1. set_dirty() is called (or continuous mode is active).
 *   2. compute_frame() calls dispatch_core_async on the executor, stores the
 *      returned FenceID, and returns immediately. No GPU work blocks the caller.
 *   3. On each subsequent compute_frame() while m_pending_fence is live,
 *      ShaderFoundry::is_fence_signaled is polled. While not signaled the call
 *      is a no-op.
 *   4. Once signaled, readback_primary / readback_aux collect the result,
 *      on_complete callbacks are fired with a GpuComputeContext, and the fence
 *      is cleared. If continuous mode is active a new dispatch is armed
 *      immediately.
 *
 * The node does not prescribe what consumers do with the result. Callbacks
 * write into whatever NDData or external storage they own. The node itself
 * stores nothing beyond the last fence.
 *
 * Usage:
 * @code
 * auto exec = std::make_shared<Yantra::ShaderExecutionContext<>>(
 *     Yantra::GpuComputeConfig { "my_shader.comp", { 256, 1, 1 }, sizeof(MyPC) });
 * exec->input(input_data).output(output_bytes).push(pc);
 *
 * auto node = std::make_shared<GpuComputeNode>(std::move(exec));
 * node->on_complete([](GpuComputeContext& ctx) {
 *     auto result = Yantra::ShaderExecutionContext<>::read_output<float>(ctx.gpu_result, 1);
 *     // write into NDData, update state, etc.
 * });
 * node->set_dirty();
 * @endcode
 */
class MAYAFLUX_API GpuComputeNode : public GpuSync {
public:
    /**
     * @brief Construct with a configured ShaderExecutionContext.
     * @param executor Configured executor. Must be non-null. Bindings, push
     *                 constants, and shader path must already be declared.
     * @param continuous If true, a new dispatch is armed immediately after each
     *                   completion. If false, the node idles until set_dirty()
     *                   is called again.
     */
    explicit GpuComputeNode(
        std::shared_ptr<Yantra::ShaderExecutionContext<>> executor,
        bool continuous = false);

    ~GpuComputeNode() override = default;

    /**
     * @brief Arm a dispatch on the next compute_frame() call.
     *
     * No-op if a dispatch is already pending. In continuous mode this is
     * called automatically after each completion and rarely needs to be
     * called externally.
     */
    void set_dirty() { m_dirty = true; }

    /**
     * @brief Returns true if a dispatch has been submitted and not yet completed.
     */
    [[nodiscard]] bool is_pending() const
    {
        return m_pending_fence != Portal::Graphics::INVALID_FENCE;
    }

    /**
     * @brief Returns true if the last dispatch completed and callbacks have fired.
     *
     * Resets to false when a new dispatch is armed.
     */
    [[nodiscard]] bool is_ready() const { return m_ready; }

    /**
     * @brief Register a callback fired once per completed dispatch.
     * @param callback Receives a GpuComputeContext carrying the GpuChannelResult.
     */
    void on_complete(const TypedHook<GpuComputeContext>& callback);

    /**
     * @brief Remove a previously registered on_complete callback.
     * @return True if found and removed.
     */
    bool remove_complete_hook(const TypedHook<GpuComputeContext>& callback);

    // -------------------------------------------------------------------------
    // GpuSync interface
    // -------------------------------------------------------------------------

    /**
     * @brief Drive the async dispatch lifecycle.
     *
     * If dirty and no dispatch is pending: arms a new dispatch via
     * dispatch_core_async, clears dirty, clears ready.
     * If a dispatch is pending: polls the fence. On signal, collects the
     * result, fires on_complete callbacks, sets ready. If continuous, arms
     * the next dispatch immediately.
     */
    void compute_frame() override;

    [[nodiscard]] bool needs_gpu_update() const override { return false; }
    void clear_gpu_update_flag() override { }

    // -------------------------------------------------------------------------
    // Node interface stubs
    // -------------------------------------------------------------------------

    void update_context(double value) override;

    NodeContext& get_last_context() override
    {
        update_context(0.0);
        return *m_context_storage;
    }

    void notify_tick(double value) override;
    void save_state() override { }
    void restore_state() override { }

private:
    std::shared_ptr<Yantra::ShaderExecutionContext<>> m_executor;
    bool m_continuous { false };
    bool m_dirty { false };
    bool m_ready { false };

    Portal::Graphics::FenceID m_pending_fence { Portal::Graphics::INVALID_FENCE };

    std::vector<TypedHook<GpuComputeContext>> m_complete_callbacks;

    Yantra::GpuChannelResult m_last_result;
    mutable std::unique_ptr<GpuComputeContext> m_context_storage;
};

} // namespace MayaFlux::Nodes::GpuSync
