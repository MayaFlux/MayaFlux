#pragma once

#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @class HampelFilter
 * @brief Holds the last accepted value when a new sample looks like an
 *        isolated outlier relative to its recent neighbors.
 *
 * Implements the Hampel identifier: a candidate is rejected when its
 * deviation from the median of a recent window exceeds threshold_mad
 * scaled median absolute deviations. This is the standard robust
 * alternative to a mean/standard-deviation outlier test, chosen
 * because a single genuine spike should not be allowed to inflate the
 * threshold that screens for itself: one wild sample shifts a window's
 * mean and stddev substantially, but only shifts a median by at most
 * one rank. The hold-last-value response on rejection is the filtering
 * variant of the Hampel identifier, as opposed to flag-only detection.
 *
 * Differential's derivatives amplify noise: a single bad raw sample that
 * survives upstream smoothing (Stochastic::Estimate) still gets
 * multiplied by 1/dt^2 or 1/dt^3, producing one wildly spiking
 * acceleration or jerk reading even though the underlying motion was
 * smooth. HampelFilter screens for that specific shape of problem, an
 * isolated one-sample spike in an already-differentiated signal, which
 * is a different concern from Estimate's job of characterizing an
 * evolving raw stream's noise floor. HampelFilter has no notion of a
 * learned floor and does not filter a raw signal; it only asks whether
 * one candidate value is consistent with its immediate recent history.
 *
 * On rejection, this returns the last accepted value rather than the
 * candidate, silently. This adds up to one sample of lag on a genuine
 * fast transient that happens to also look statistically unusual,
 * which is the deliberate tradeoff: a caller feeding this into a
 * visual parameter or gesture classifier is generally better served
 * by a held value than a single-frame glitch. A caller that needs to
 * know a rejection happened rather than have it silently smoothed over
 * should not use this class; it is intentionally not a flag-only tool.
 *
 * @tparam T Sample type. Must support operator-, and the resulting
 *           difference type must be usable with scalar_t<T> reductions
 *           (a plain arithmetic value directly, a glm vector via its
 *           magnitude). See magnitude_of() below for the exact rule.
 */
template <typename T>
class HampelFilter {
public:
    /**
     * @brief Construct a filter
     * @param window Number of recent accepted samples to test new
     *        candidates against, minimum 3 since MAD needs at least a
     *        few points to be meaningful
     * @param threshold_mad Number of scaled-MAD units beyond which a
     *        candidate is rejected, typically 3.0 to 4.0
     * @param max_consecutive_rejections Forces the next candidate to be
     *        accepted unconditionally, and clears history to restart
     *        the window from it, once this many candidates in a row
     *        have been rejected. Without this, a real sustained change
     *        in the signal (which any consecutive Differential reading
     *        during real motion looks like relative to an older,
     *        now-stale window) can never be re-accepted: once every
     *        recent candidate is rejected against a frozen median, the
     *        filter locks onto whatever value it last accepted and
     *        holds it indefinitely, since a rejected candidate is never
     *        added to the window that future candidates are tested
     *        against. Default 3, since three consecutive genuine
     *        outliers from the same underlying cause is already an
     *        unusual coincidence; more than that is far more likely to
     *        be a real change the window has fallen behind.
     */
    explicit HampelFilter(size_t window = 8, double threshold_mad = 3.5,
        size_t max_consecutive_rejections = 3)
        : m_history(window < 3 ? 3 : window)
        , m_threshold_mad(threshold_mad)
        , m_max_consecutive_rejections(max_consecutive_rejections)
        , m_last_good(T {})
        , m_has_history(false)
    {
    }

    /**
     * @brief Test a candidate value and return the value to actually use
     * @param candidate Newly computed value, typically straight out of
     *        a Kinesis::Differential function
     * @return candidate if it looks consistent with recent history, or
     *         the last accepted value if candidate looks like an
     *         isolated spike
     *
     * The first call and the next (window - 1) calls always accept,
     * since MAD is not meaningful against a window that has not yet
     * filled with real data; a held value from an uninitialized filter
     * would be an arbitrary default, not a real prior reading.
     */
    T accept(const T& candidate)
    {
        if (!m_has_history) {
            m_history.push(candidate);
            m_last_good = candidate;
            ++m_sample_count;
            if (m_sample_count >= m_history.capacity())
                m_has_history = true;
            return candidate;
        }

        const auto view = m_history.linearized_view();

        std::vector<double> magnitudes;
        magnitudes.reserve(view.size());
        for (const auto& v : view)
            magnitudes.push_back(magnitude_of(v));

        std::vector<double> sorted = magnitudes;
        std::ranges::sort(sorted);
        const double median_mag = sorted[sorted.size() / 2];

        std::vector<double> deviations;
        deviations.reserve(sorted.size());
        for (double m : sorted)
            deviations.push_back(std::abs(m - median_mag));
        std::ranges::sort(deviations);
        const double mad = deviations[deviations.size() / 2] * 1.4826;

        const double candidate_mag = magnitude_of(candidate);
        const bool looks_like_spike = (mad > 1e-12)
            && (std::abs(candidate_mag - median_mag) / mad > m_threshold_mad);

        if (looks_like_spike && m_consecutive_rejections < m_max_consecutive_rejections) {
            ++m_consecutive_rejections;
            return m_last_good;
        }

        if (looks_like_spike) {
            m_history.reset();
            m_sample_count = 0;
            m_has_history = false;
        }

        m_consecutive_rejections = 0;
        m_history.push(candidate);
        m_last_good = candidate;
        ++m_sample_count;
        if (m_sample_count >= m_history.capacity())
            m_has_history = true;
        return candidate;
    }

    /**
     * @brief Reset to uninitialized state
     *
     * Call on a known discontinuity (device reconnect, deliberate
     * large jump the caller does not want screened as a spike) so the
     * next window's worth of candidates re-accepts unconditionally
     * rather than being tested against now-stale history.
     */
    void reset()
    {
        m_history.reset();
        m_last_good = T {};
        m_has_history = false;
        m_sample_count = 0;
        m_consecutive_rejections = 0;
    }

    /**
     * @brief Current last accepted value
     */
    [[nodiscard]] const T& last_good() const { return m_last_good; }

    /**
     * @brief Whether the filter has enough history to actually screen candidates
     *
     * False during the initial window-filling period, when accept()
     * always returns its argument unconditionally.
     */
    [[nodiscard]] bool is_active() const { return m_has_history; }

private:
    /**
     * @brief Scalar reduction used for the MAD comparison
     *
     * Plain arithmetic T reduces to its absolute value directly.
     * glm vector T reduces to its length. This means HampelFilter
     * screens on magnitude of change, not direction: a candidate whose
     * magnitude is consistent with recent history is accepted even if
     * its direction differs, which is deliberate, since a real sharp
     * turn is a direction change with unremarkable magnitude and
     * should not be screened out as if it were a spike.
     */
    template <typename U>
    static double magnitude_of(const U& v) noexcept
    {
        if constexpr (requires { glm::length(v); }) {
            return static_cast<double>(glm::length(v));
        } else {
            return static_cast<double>(std::abs(v));
        }
    }

    Memory::HistoryBuffer<T> m_history;
    double m_threshold_mad;
    size_t m_max_consecutive_rejections;
    T m_last_good;
    bool m_has_history;
    size_t m_sample_count { 0 };
    size_t m_consecutive_rejections { 0 };
};

} // namespace MayaFlux::Kinesis
