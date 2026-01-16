#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GpuSyncNode
 * @brief Base abstraction for CPU-GPU coordinated nodes
 *
 * Captures the common pattern of "CPU computation at VISUAL_RATE
 * that produces GPU-bindable data". Provides:
 * - Frame synchronization (`compute_frame()` pure virtual)
 * - Unified process_sample/batch interface
 * - Error logging and state management
 * - Optional layout caching for Kakshya bindings
 *
 * Does NOT own data storage (too heterogeneous).
 * Subclasses (TextureNode, GeometryWriterNode) own their buffers.
 */
class MAYAFLUX_API GpuSync : public Node {
public:
    ~GpuSync() override = default;

    /**
     * @brief Compute GPU data for this frame
     *
     * Called once per VISUAL_RATE tick. Subclasses populate their
     * respective buffers (pixel_buffer, vertex_buffer, readback_data).
     */
    virtual void compute_frame() = 0;

    /**
     * @brief Single sample processing hook
     * @param input Unused for most GPU sync nodes
     * @return Always 0.0 (GPU nodes don't produce scalar audio output)
     */
    double process_sample(double /*input*/) override
    {
        compute_frame();
        return 0.0;
    }

    /**
     * @brief Batch processing for GPU nodes
     * @param num_samples Number of frames to process
     * @return Zero-filled vector (no audio output)
     */
    std::vector<double> process_batch(unsigned int num_samples) override
    {
        for (unsigned int i = 0; i < num_samples; ++i) {
            compute_frame();
        }
        return std::vector<double>(num_samples, 0.0);
    }

    /**
     * @brief Mark if this node needs GPU binding update
     * @return True if data layout or count changed
     */
    [[nodiscard]] virtual bool needs_gpu_update() const = 0;

    /**
     * @brief Clear the "needs update" flag after GPU binding
     */
    virtual void clear_gpu_update_flag() = 0;

protected:
    /**
     * @brief GPU sync nodes don't emit tick callbacks
     */
    void notify_tick(double /*value*/) override { }
};

} // namespace MayaFlux::Nodes::GpuSync
