#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class Constant
 * @brief Zero-overhead scalar source node that emits a fixed value on every tick
 *
 * Constant is the identity element of the node graph: it ignores all input,
 * holds a single double, and returns it unconditionally from every processing
 * call. No state history, no coefficient arrays, no oscillator phase —
 * just a number in, number out.
 *
 * Primary uses:
 * - bias a signal by a fixed amount (DC offset injection)
 * - Static parameter supply for parameter-mapping systems (e.g., feed a fixed
 * - frequency into ResonatorNetwork::map_parameter("frequency", ...))
 * - Push constants into GPU shader pipelines via NodeTextureBuffer
 * - Test fixture / mock node that stands in for any scalar source
 * - Sentinel value in node graphs during live-coding sessions where a real
 *   source node has not yet been wired in
 *
 * The node is intentionally minimal. save_state()/restore_state() snapshot the
 * value so that buffer snapshot cycles remain consistent. notify_tick() fires
 * the standard callback chain so that on_tick() listeners work identically to
 * any other node.
 */
class MAYAFLUX_API Constant final : public Node {
public:
    //-------------------------------------------------------------------------
    // Construction
    //-------------------------------------------------------------------------

    /**
     * @brief Construct with an initial constant value
     * @param value Scalar to emit on every process_sample() / process_batch() call
     */
    explicit Constant(double value = 0.0);

    //-------------------------------------------------------------------------
    // Core processing
    //-------------------------------------------------------------------------

    /**
     * @brief Return the constant value, ignoring input
     * @param input Ignored; present only to satisfy the Node interface
     * @return The stored constant
     *
     * Updates m_last_output, fires notify_tick(), and returns the value.
     * The input parameter is accepted but never read: Constant is a pure source.
     */
    double process_sample(double input = 0.0) override;

    /**
     * @brief Fill a buffer with the constant value
     * @param num_samples Number of samples to generate
     * @return Vector of num_samples copies of the constant
     *
     * Each element is produced via process_sample(0.0) so that per-sample
     * callbacks fire correctly for every position in the batch.
     */
    std::vector<double> process_batch(unsigned int num_samples) override;

    //-------------------------------------------------------------------------
    // Parameter control
    //-------------------------------------------------------------------------

    /**
     * @brief Update the emitted value
     * @param value New constant to emit from the next process call onward
     */
    void set_constant(double value);

    /**
     * @brief Read the current constant value without triggering processing
     */
    [[nodiscard]] double get_constant() const { return m_value; }

    //-------------------------------------------------------------------------
    // State snapshot (used by buffer processing chains)
    //-------------------------------------------------------------------------

    /**
     * @brief Snapshot the current value for later restoration
     *
     * Stores m_value so that isolated buffer processing
     * (NodeSourceProcessor, FilterProcessor snapshot paths) does not corrupt
     * the live value seen by other consumers.
     */
    void save_state() override;

    /**
     * @brief Restore value from last save_state() call
     */
    void restore_state() override;

    //-------------------------------------------------------------------------
    // Context / callback support
    //-------------------------------------------------------------------------

    /**
     * @brief Return the cached NodeContext from the last process_sample() call
     */
    NodeContext& get_last_context() override;

protected:
    /**
     * @brief Update m_context with the latest value
     * @param value Most recently emitted sample
     */
    void update_context(double value) override;

    /**
     * @brief Fire all registered on_tick() and on_tick_if() callbacks
     * @param value Most recently emitted sample
     */
    void notify_tick(double value) override;

private:
    /**
     * @brief Minimal concrete context for Constant — no extra fields beyond the base
     */
    struct ConstantContext final : NodeContext {
        ConstantContext()
            : NodeContext(0.0, typeid(Constant).name())
        {
        }
    };

    double m_value;
    double m_saved_value {};

    ConstantContext m_context;
};

} // namespace MayaFlux::Nodes
