#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class FeedbackBuffer
 * @brief AudioBuffer that owns and publishes a history ring
 *
 * The ring holds the values the buffer's feedback processors carry from one
 * sample to the next, newest at index 0. Processors attached to a
 * FeedbackBuffer use this ring; processors on any other AudioBuffer keep
 * their own. The default processor is a single-tap FeedbackProcessor:
 * output = input + feedback * (value feed_samples ago).
 */
class MAYAFLUX_API FeedbackBuffer : public AudioBuffer {
public:
    /**
     * @param channel_id Channel this buffer serves
     * @param num_samples Samples per block
     * @param feedback Gain of the default processor
     * @param feed_samples Lag of the default processor, in samples (minimum 1)
     */
    FeedbackBuffer(uint32_t channel_id, uint32_t num_samples,
        float feedback = 0.5F, uint32_t feed_samples = 512);

    using AudioBuffer::clone_to;

    [[nodiscard]] inline float get_feedback() const { return m_feedback_amount; }

    void set_feedback(float amount);

    [[nodiscard]] inline Memory::HistoryBuffer<double>& get_history_buffer() { return m_history; }

    [[nodiscard]] inline const Memory::HistoryBuffer<double>& get_history_buffer() const { return m_history; }

    void process_default() override;

    void set_feed_samples(uint32_t samples);

    [[nodiscard]] inline uint32_t get_feed_samples() const { return m_feed_samples; }

    /**
     * @brief Clone for another channel with its own ring and its own processor state
     */
    std::shared_ptr<AudioBuffer> clone_to(uint32_t channel) override;

protected:
    std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    float m_feedback_amount;
    uint32_t m_feed_samples;
    Memory::HistoryBuffer<double> m_history;
};

/**
 * @class FeedbackProcessor
 * @brief Combines each sample with lagged values of its own carried stream
 *
 * Per sample the processor reads one value per tap from a ring of carried
 * values, calls the combiner with the current sample, those tap values and
 * the coefficient array, writes the result to the buffer and pushes the
 * carried value into the ring. Lags start at one sample, so recursion
 * within a block is possible. State (ring, coefficient array, lag ramps) is
 * kept per attached buffer, so one processor can serve many buffers and
 * clones never share history.
 *
 * Works on any AudioBuffer. On a FeedbackBuffer the ring is the buffer's own.
 */
class MAYAFLUX_API FeedbackProcessor : public BufferProcessor {
public:
    /**
     * @brief Result of a combiner that carries a different value than it outputs
     *
     * out goes to the buffer, carry goes into the ring. Feedback carries the
     * output, feedforward carries the input.
     */
    struct Step {
        double out;
        double carry;
    };

    /**
     * @brief Combiner returning one value, written to the buffer and carried
     * @param x Current sample
     * @param taps Lagged values, one per tap
     * @param coefficients This buffer's coefficient array, free to read and write
     */
    using Combine = std::function<double(double, std::span<const double>, std::span<double>)>;

    /**
     * @brief Combiner returning an output and a carried value
     */
    using CombineStep = std::function<Step(double, std::span<const double>, std::span<double>)>;

    /**
     * @brief Single tap, output = input + feedback * tap
     * @param feedback Gain
     * @param feed_samples Lag in samples, minimum 1
     */
    FeedbackProcessor(float feedback = 0.5F, uint32_t feed_samples = 512);

    /**
     * @param combine Combiner
     * @param lags Tap lags in samples, fractional lags are interpolated linearly
     * @param coefficients Initial coefficient array, copied to each attached buffer
     */
    FeedbackProcessor(Combine combine, const std::vector<double>& lags, std::vector<double> coefficients = {});

    FeedbackProcessor(CombineStep combine, const std::vector<double>& lags, std::vector<double> coefficients = {});

    void processing_function(const std::shared_ptr<Buffer>& buffer) override;
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

    void set_feedback(float amount) { m_feedback_amount = amount; }

    [[nodiscard]] float get_feedback() const { return m_feedback_amount; }

    /**
     * @brief Lag of the first tap, in samples
     */
    void set_feed_samples(uint32_t samples);

    [[nodiscard]] uint32_t get_feed_samples() const { return m_feed_samples; }

    void set_combine(Combine combine);

    void set_combine(CombineStep combine);

    /**
     * @brief Replace all taps with fixed lags in samples
     */
    void set_lags(const std::vector<double>& lags);

    /**
     * @brief Drive one tap's lag from a TimeMap
     * @param tap Tap index
     * @param seconds Lag in seconds as a function of seconds processed so far
     * @param max_seconds Largest lag the map may return, sizes the ring
     *
     * Evaluated once per block and ramped per sample across the block.
     * @throws std::out_of_range if tap is not an existing tap
     */
    void set_lag(size_t tap, Kinesis::TimeMap seconds, double max_seconds);

    /**
     * @brief Set the initial coefficient array and reset every attached buffer's copy
     */
    void set_coefficients(std::vector<double> coefficients);

    /**
     * @brief The coefficient array of one attached buffer, empty if it is not attached
     */
    [[nodiscard]] std::span<double> coefficients(const std::shared_ptr<Buffer>& buffer);

    /**
     * @brief Same configuration, no attached state
     */
    [[nodiscard]] std::shared_ptr<FeedbackProcessor> clone() const;

private:
    struct Tap {
        double samples { 1.0 };
        std::shared_ptr<const Kinesis::TimeMap> map;
        double max_samples { 1.0 };
    };

    struct State {
        Memory::HistoryBuffer<double> own { 1 };
        Memory::HistoryBuffer<double>* ring { nullptr };
        std::vector<double> coefs;
        std::vector<double> values;
        std::vector<double> lag_prev;
        std::vector<double> lag_now;
        uint64_t elapsed { 0 };
        bool primed { false };
    };

    State& state_for(const std::shared_ptr<Buffer>& buffer);

    void recompute_capacity();

    static double read(const Memory::HistoryBuffer<double>& ring, double lag);

    float m_feedback_amount { 0.5F };
    uint32_t m_feed_samples { 512 };
    bool m_default_combine { false };

    Combine m_combine;
    CombineStep m_step;
    std::vector<Tap> m_taps;
    std::vector<double> m_coefficients;
    size_t m_capacity { 1 };

    std::unordered_map<const Buffer*, State> m_states;
};

} // namespace MayaFlux::Buffers
