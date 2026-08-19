#pragma once

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Kinesis::Stochastic {

/**
 * @enum EstimateModel
 * @brief Strategies for characterizing an evolving stream's statistical behavior
 *
 * Stochastic generates a sequence with chosen statistical character.
 * Estimate consumes an arriving sequence and characterizes its statistical
 * character as it evolves, since a live stream from a physical source
 * (a sensor, a device report, any per-frame sample) does not carry a
 * fixed, known noise floor, spread, or trend the way a synthetic signal
 * does. Both are stateful processes that evolve over successive calls;
 * this enum plays the role Algorithm plays for Stochastic, selecting how
 * that evolution is computed rather than what it produces.
 */
enum class EstimateModel : uint8_t {
    ROLLING_VARIANCE, // Variance and floor tracked over a fixed recent window
    EWM_VARIANCE, // Variance and floor tracked via exponential weighting, unbounded memory depth
    MEDIAN_ABSOLUTE_DEVIATION, // Floor tracked via MAD, robust to single outlier spikes
    QUIET_PERIOD_FLOOR, // Floor only updates during self-detected low-activity stretches
    TREND // Running linear trend and the variance not explained by it
};

/**
 * @struct EstimateState
 * @brief Persistent state for an Estimate instance
 *
 * Mirrors GeneratorState in role: a stateful process that evolves over
 * successive calls needs somewhere to keep that evolution, exposed for
 * analysis, visualization, or external nudging rather than hidden
 * entirely inside the class. Not every field is meaningful for every
 * EstimateModel; unused fields for a given model stay at their default.
 */
struct EstimateState {
    double running_mean { 0.0 };
    double running_variance { 0.0 };
    double floor { 0.0 };
    double trend_slope { 0.0 };
    double trend_explained_ratio { 0.0 };
    double last_raw_sample { 0.0 };
    uint64_t sample_count { 0 };

    void reset()
    {
        running_mean = 0.0;
        running_variance = 0.0;
        floor = 0.0;
        trend_slope = 0.0;
        trend_explained_ratio = 0.0;
        last_raw_sample = 0.0;
        sample_count = 0;
    }
};

/**
 * @class Estimate
 * @brief Stateful statistical characterization of an evolving scalar stream
 *
 * Provides mathematical primitives for characterizing controlled
 * uncertainty in an arriving signal, across all computational domains.
 * This is the read direction of what Stochastic is for the write
 * direction: Stochastic produces a signal with chosen statistical
 * character, Estimate consumes a signal and characterizes its
 * statistical character, updating that characterization as the
 * stream's behavior itself changes.
 *
 * ## Architectural Philosophy
 * Treats stream characterization as fundamental mathematical
 * infrastructure rather than domain-specific processing. The same
 * primitives that learn a tablet's pressure noise floor can learn a
 * camera-derived tracking point's jitter, an analysis feature's drift,
 * or any other per-frame scalar's evolving statistical behavior. The
 * numbers themselves are discipline-agnostic.
 *
 * ## Model Categories
 *
 * **Windowed** (bounded recent memory):
 * - ROLLING_VARIANCE: mean and variance over the last N samples
 * - MEDIAN_ABSOLUTE_DEVIATION: median-based floor, robust to spikes
 * - QUIET_PERIOD_FLOOR: floor only updates when the window looks calm
 *   by its own recent standard
 * - TREND: linear trend and residual variance over the last N samples
 *
 * **Unbounded** (exponentially weighted memory):
 * - EWM_VARIANCE: mean and variance tracked with no fixed window depth,
 *   older samples fade rather than drop off a cliff
 *
 * ## Usage Patterns
 *
 * Learning a noise floor:
 * ```cpp
 * Estimate est(EstimateModel::EWM_VARIANCE);
 * for (auto sample : incoming_stream) {
 *     double floor = est.update(sample);
 *     double conf = est.confidence(sample - est.state().last_raw_sample);
 * }
 * ```
 *
 * One-shot characterization of an already-captured window:
 * ```cpp
 * double v = Estimate::variance(samples);
 * double trend = Estimate::trend_explained_ratio(samples);
 * ```
 *
 * @note Thread-unsafe for maximum performance, matching Stochastic.
 * Use separate instances per stream per thread.
 */
class MAYAFLUX_API Estimate {
public:
    /**
     * @brief Constructs an estimator with the specified model
     * @param model Characterization strategy (default: EWM_VARIANCE)
     * @param adapt_rate Blend rate for EWM_VARIANCE, ignored by other
     *        models. Smaller values remember longer, larger values
     *        adapt faster. Range (0, 1].
     * @param window Sample count for windowed models (ROLLING_VARIANCE,
     *        MEDIAN_ABSOLUTE_DEVIATION, QUIET_PERIOD_FLOOR, TREND).
     *        Ignored by EWM_VARIANCE.
     */
    explicit Estimate(EstimateModel model = EstimateModel::EWM_VARIANCE,
        double adapt_rate = 0.05, size_t window = 32);

    /**
     * @brief Changes active model
     * @param model New characterization model
     *
     * Resets internal state when switching models, matching
     * Stochastic::set_algorithm.
     */
    void set_model(EstimateModel model);

    /**
     * @brief Gets current model
     */
    [[nodiscard]] inline EstimateModel get_model() const { return m_model; }

    /**
     * @brief Sets the adapt rate used by EWM_VARIANCE
     */
    void set_adapt_rate(double rate) { m_adapt_rate = rate; }

    /**
     * @brief Gets the current adapt rate
     */
    [[nodiscard]] double get_adapt_rate() const { return m_adapt_rate; }

    /**
     * @brief Sets the window size used by windowed models
     *
     * Clears any accumulated window contents; the estimate rebuilds
     * from the next window worth of samples.
     */
    void set_window(size_t window);

    /**
     * @brief Gets the current window size
     */
    [[nodiscard]] size_t get_window() const { return m_window; }

    /**
     * @brief Feed one new sample, updating the running estimate
     * @param sample Raw value for this step
     * @return Current floor after incorporating this sample. For TREND,
     *         returns the current residual (non-trend) standard deviation.
     */
    double update(double sample);

    /**
     * @brief Confidence that a delta from the last sample is signal, not floor
     * @param delta Difference to evaluate, typically a computed velocity
     *        or step from Kinesis::Differential
     * @return 0.0 (indistinguishable from floor) to 1.0 (well above floor)
     *
     * Not a hard threshold. A caller wanting a binary decision picks
     * their own cutoff against this; Estimate only reports where the
     * delta sits relative to the learned floor.
     */
    [[nodiscard]] double confidence(double delta) const;

    /**
     * @brief Current learned floor
     *
     * For TREND, this is the residual (non-trend) standard deviation
     * rather than a noise floor in the windowed-variance sense.
     */
    [[nodiscard]] double floor() const { return m_state.floor; }

    /**
     * @brief Resets internal state
     *
     * Memoryless usage is unaffected since Estimate has none; this
     * clears all accumulated running state and window contents.
     */
    void reset();

    /**
     * @brief Gets current internal state
     * @return Read-only reference to estimator state
     *
     * Exposes complete internal state for analysis, visualization,
     * debugging, or extracting the learned characterization for use
     * elsewhere (e.g. seeding a second Estimate instance's floor).
     */
    [[nodiscard]] const EstimateState& state() const { return m_state; }

    /**
     * @brief Gets mutable internal state
     * @return Mutable reference to estimator state
     *
     * Enables direct manipulation for seeding a floor from prior
     * analysis, resetting after a known discontinuity (device
     * reconnect, deliberate large jump), or externally nudging
     * the estimate.
     */
    [[nodiscard]] EstimateState& state_mutable() { return m_state; }

    // ========================================================================
    // Stateless span characterization
    // ========================================================================
    //
    // One-shot estimates over a span the caller already holds, with no
    // persistent state, for the case where an Estimate instance's evolving
    // memory is not wanted, e.g. characterizing a single already-captured
    // window rather than tracking a live stream call to call.

    /**
     * @brief Sample variance of a span
     * @param samples Values to analyze
     * @return Variance, or 0.0 for spans of size < 2
     */
    [[nodiscard]] static double variance(std::span<const double> samples) noexcept;

    /**
     * @brief Standard deviation of a span
     * @param samples Values to analyze
     * @return sqrt(variance(samples))
     */
    [[nodiscard]] static double stddev(std::span<const double> samples) noexcept;

    /**
     * @brief Median absolute deviation of a span
     * @param samples Values to analyze
     * @return Median of |x_i - median(samples)|, scaled by 1.4826 to be
     *         a consistent estimator of standard deviation under a
     *         normal distribution assumption. Robust to single-sample
     *         spikes in a way variance is not, since one wild outlier
     *         can dominate a variance estimate but only shifts a
     *         median by at most one rank.
     */
    [[nodiscard]] static double median_absolute_deviation(std::span<const double> samples) noexcept;

    /**
     * @brief Flags samples whose deviation from the median exceeds a threshold
     * @param samples Values to analyze
     * @param threshold_mad Number of scaled-MAD units beyond which a
     *        sample is flagged, typically 2.5 to 3.5 for physical
     *        sensor data
     * @return Indices into samples considered outliers
     *
     * Shape-aware in the sense that a single wild sample and a run of
     * several consistent high samples are distinguished by the caller
     * inspecting which indices come back, not by this function alone:
     * an isolated flagged index surrounded by unflagged ones reads
     * differently from several consecutive flagged indices, and that
     * read is left to the caller since the correct interpretation
     * depends on domain (a genuine fast transition vs. a dropout burst).
     */
    [[nodiscard]] static std::vector<size_t> flag_outliers(
        std::span<const double> samples, double threshold_mad = 3.0) noexcept;

    /**
     * @brief Fraction of a span's variance attributable to its linear trend
     * @param samples Values to analyze
     * @return 1.0 when the span is well explained by a straight line
     *         from first to last sample, closer to 0.0 when the span's
     *         variance is dominated by fluctuation around that line
     *         rather than the trend itself
     *
     * A cheap signal/noise split for a window: high trend-explained
     * ratio means the window looks like real directed change, low
     * means the window looks like jitter around a roughly fixed point.
     */
    [[nodiscard]] static double trend_explained_ratio(std::span<const double> samples) noexcept;

    /**
     * @brief Linear trend slope across a span
     * @param samples Values to analyze
     * @return Least-squares slope in units of (value per sample index),
     *         0.0 for spans of size < 2
     */
    [[nodiscard]] static double trend_slope(std::span<const double> samples) noexcept;

private:
    double update_rolling_variance(double sample);
    double update_ewm_variance(double sample);
    double update_mad(double sample);
    double update_quiet_period(double sample);
    double update_trend(double sample);

    EstimateModel m_model;
    double m_adapt_rate;
    size_t m_window;

    EstimateState m_state;

    // ROLLING_VARIANCE, MEDIAN_ABSOLUTE_DEVIATION, QUIET_PERIOD_FLOOR, and
    // TREND keep a bounded window of raw samples via HistoryBuffer, which
    // is fixed-capacity after construction with O(1) push and no per-call
    // allocation, matching the real-time constraint this class runs under.
    // EWM_VARIANCE does not use m_history at all.
    Memory::HistoryBuffer<double> m_history;
};

} // namespace MayaFlux::Kinesis::Stochastic
