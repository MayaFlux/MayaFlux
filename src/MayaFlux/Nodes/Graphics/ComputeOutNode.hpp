#pragma once

#include "GpuSync.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
} // namespace MayaFlux::Buffers

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class ComputeOutContext
 * @brief Context for ComputeOutNode - provides readback buffer access
 */
class MAYAFLUX_API ComputeOutContext : public NodeContext, public GpuVectorData {
public:
    ComputeOutContext(double value, std::span<const float> readback_data, size_t element_count)
        : NodeContext(value, typeid(ComputeOutContext).name())
        , GpuVectorData(readback_data)
        , element_count(element_count)
    {
    }

    size_t element_count;

protected:
    friend class ComputeOutNode;
};

/**
 * @class ComputeOutNode
 * @brief Node that reads back data from GPU buffer to CPU
 *
 * Facilitates GPU → CPU data transfer by downloading compute shader results
 * into a CPU-accessible vector. Useful for feedback loops where GPU computation
 * results need to influence CPU-side node processing or decision-making.
 *
 * Usage:
 *   // GPU computes particle collisions
 *   auto collision_buffer = std::make_shared<VKBuffer>(...);
 *   auto collision_node = std::make_shared<ComputeOutNode>(collision_buffer, 1);
 *
 *   // Node reads collision count from GPU
 *   double collision_count = collision_node->process_sample();
 */
class MAYAFLUX_API ComputeOutNode : public GpuSync {
public:
    /**
     * @brief Construct with GPU buffer and element count
     * @param buffer GPU buffer to read from
     * @param element_count Number of double elements to read
     */
    ComputeOutNode(const std::shared_ptr<Buffers::VKBuffer>& buffer, size_t element_count);

    void compute_frame() override;

    /**
     * @brief Get full readback array
     * @return Reference to downloaded data
     */
    [[nodiscard]] const std::vector<double>& get_readback_data() const { return m_readback_data; }

    /**
     * @brief Get specific element from readback data
     * @param index Element index
     * @return Element value
     */
    [[nodiscard]] double get_element(size_t index) const;

    /**
     * @brief Get number of elements
     * @return Element count
     */
    [[nodiscard]] size_t get_element_count() const { return m_element_count; }

    /**
     * @brief Get GPU buffer
     * @return GPU buffer reference
     */
    [[nodiscard]] std::shared_ptr<Buffers::VKBuffer> get_gpu_buffer() const { return m_gpu_buffer; }

    [[nodiscard]] bool needs_gpu_update() const override
    {
        return false; // Readback nodes don't trigger GPU updates
    }

    void clear_gpu_update_flag() override { }

    /**
     * @brief Update context with current readback data
     * @param value Current sample value
     */
    void update_context(double value) override;

    /**
     * @brief Get last created context object
     * @return Reference to last ComputeOutContext
     */
    NodeContext& get_last_context() override { return m_context; }

protected:
    mutable std::vector<float> m_readback_float_buffer;
    ComputeOutContext m_context { 0.0, {}, 0 };

private:
    std::shared_ptr<Buffers::VKBuffer> m_gpu_buffer;
    std::vector<double> m_readback_data;
    size_t m_element_count;
};

} // namespace MayaFlux::Nodes
