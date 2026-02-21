#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FeedbackBuffer
 * @brief Buffer with temporal memory for recursive processing
 *
 * FeedbackBuffer extends AudioBuffer with a HistoryBuffer that maintains
 * the previous processing state, enabling delay-line feedback and recursive
 * algorithms. The history buffer provides proper temporal indexing where
 * [0] = most recent sample and [k] = k samples ago.
 *
 * The feedback path is:
 *   output[n] = input[n] + feedback_amount * history[feed_samples]
 *
 * For filtering in the feedback path, attach a FilterProcessor to the
 * buffer's processing chain rather than relying on hardcoded averaging.
 */
class MAYAFLUX_API FeedbackBuffer : public AudioBuffer {
public:
    /**
     * @brief Construct feedback buffer
     * @param channel_id Audio channel assignment
     * @param num_samples Buffer size in samples
     * @param feedback Feedback coefficient (0.0 to 1.0)
     * @param feed_samples Delay length in samples
     */
    FeedbackBuffer(uint32_t channel_id, uint32_t num_samples,
        float feedback = 0.5F, uint32_t feed_samples = 512);

    /**
     * @brief Get feedback coefficient
     */
    [[nodiscard]] inline float get_feedback() const { return m_feedback_amount; }

    /**
     * @brief Set feedback coefficient
     * @param amount Feedback coefficient (0.0 to 1.0)
     *
     * Propagates to the default processor if one is attached.
     */
    void set_feedback(float amount);

    /**
     * @brief Get mutable access to the history buffer
     * @return Reference to the HistoryBuffer storing previous state
     */
    inline Memory::HistoryBuffer<double>& get_history_buffer() { return m_history; }

    /**
     * @brief Get read-only access to the history buffer
     * @return Const reference to the HistoryBuffer storing previous state
     */
    [[nodiscard]] inline const Memory::HistoryBuffer<double>& get_history_buffer() const { return m_history; }

    void process_default() override;

    /**
     * @brief Set delay length in samples
     * @param samples New delay length
     *
     * Reconstructs the history buffer. Previous state is lost.
     */
    void set_feed_samples(uint32_t samples);

    /**
     * @brief Get delay length in samples
     */
    [[nodiscard]] inline uint32_t get_feed_samples() const { return m_feed_samples; }

protected:
    std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    float m_feedback_amount;
    uint32_t m_feed_samples;
    Memory::HistoryBuffer<double> m_history;
};

/**
 * @class FeedbackProcessor
 * @brief Processor implementing delay-line feedback via HistoryBuffer
 *
 * Applies a simple delay-line feedback algorithm:
 *   output[n] = input[n] + feedback_amount * delayed_sample
 *
 * When attached to a FeedbackBuffer, uses its internal HistoryBuffer.
 * When attached to any other AudioBuffer, maintains its own HistoryBuffer.
 *
 * For filtering in the feedback loop (lowpass damping, etc.), chain a
 * FilterProcessor after this processor rather than embedding filter logic.
 */
class MAYAFLUX_API FeedbackProcessor : public BufferProcessor {
public:
    /**
     * @brief Construct feedback processor
     * @param feedback Feedback coefficient (0.0 to 1.0)
     * @param feed_samples Delay length in samples
     */
    FeedbackProcessor(float feedback = 0.5F, uint32_t feed_samples = 512);

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

    /**
     * @brief Set feedback coefficient
     */
    inline void set_feedback(float amount) { m_feedback_amount = amount; }

    /**
     * @brief Get feedback coefficient
     */
    [[nodiscard]] inline float get_feedback() const { return m_feedback_amount; }

    /**
     * @brief Set delay length in samples
     */
    void set_feed_samples(uint32_t samples);

    /**
     * @brief Get delay length in samples
     */
    [[nodiscard]] inline uint32_t get_feed_samples() const { return m_feed_samples; }

private:
    float m_feedback_amount;
    uint32_t m_feed_samples;

    Memory::HistoryBuffer<double> m_history;
    Memory::HistoryBuffer<double>* m_active_history { nullptr };
};

} // namespace MayaFlux::Buffers
