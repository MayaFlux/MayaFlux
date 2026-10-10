#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Buffers {

/**
 * @class BlockFeedbackProcessor
 * @brief Compares and transforms each whole block against an earlier block
 *
 * Where FeedbackProcessor works sample by sample, this works on the block as
 * a unit: every cycle it hands a function the current block, the block from a
 * set number of cycles ago and a coefficient array, and keeps what the
 * function leaves in the buffer for later cycles. The function may rewrite
 * the current block in place and returns one number, a feature of the block
 * (a distance, a correlation, a level against the earlier one), which is
 * stored per buffer and passed to an optional observer.
 *
 * State (retained blocks, coefficient array, last feature) is kept per
 * attached buffer. Works on any AudioBuffer. Until enough cycles have passed
 * the earlier block is zeros.
 */
class MAYAFLUX_API BlockFeedbackProcessor : public BufferProcessor {
public:
    /**
     * @brief The per-block function
     * @param current This cycle's block, free to rewrite
     * @param previous The retained block from `lag_blocks` cycles ago
     * @param coefficients This buffer's coefficient array, free to read and write
     * @return A feature of the block
     */
    using Transform = std::function<double(std::span<double>, std::span<const double>, std::span<double>)>;

    /**
     * @param transform The per-block function
     * @param lag_blocks How many cycles back the earlier block lies, minimum 1
     * @param coefficients Initial coefficient array, copied to each attached buffer
     */
    BlockFeedbackProcessor(Transform transform, size_t lag_blocks = 1, std::vector<double> coefficients = {});

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    /**
     * @brief Keep the incoming block instead of the transformed one
     * @param retain_input True retains what arrived, so the earlier block is
     *        the earlier input; false, the default, retains what the function
     *        left, so the earlier block is the earlier output
     */
    void set_retain_input(bool retain_input) { m_retain_input = retain_input; }

    [[nodiscard]] bool get_retain_input() const { return m_retain_input; }

    /**
     * @brief Called with the feature after every block
     */
    void set_observer(std::function<void(double)> observer) { m_observer = std::move(observer); }

    /**
     * @brief The feature of the last block of one attached buffer, zero if none
     */
    [[nodiscard]] double feature(const std::shared_ptr<Buffer>& buffer) const;

    /**
     * @brief The coefficient array of one attached buffer, empty if it is not attached
     */
    [[nodiscard]] std::span<double> coefficients(const std::shared_ptr<Buffer>& buffer);

    [[nodiscard]] size_t get_lag_blocks() const { return m_lag_blocks; }

    /**
     * @brief Same configuration, no attached state
     */
    [[nodiscard]] std::shared_ptr<BlockFeedbackProcessor> clone() const;

private:
    struct State {
        std::vector<std::vector<double>> blocks;
        std::vector<double> kept;
        std::vector<double> coefs;
        size_t next { 0 };
        double feature { 0.0 };
    };

    Transform m_transform;
    size_t m_lag_blocks { 1 };
    bool m_retain_input { false };
    std::vector<double> m_coefficients;
    std::function<void(double)> m_observer;

    std::unordered_map<const Buffer*, State> m_states;
};

} // namespace MayaFlux::Buffers
