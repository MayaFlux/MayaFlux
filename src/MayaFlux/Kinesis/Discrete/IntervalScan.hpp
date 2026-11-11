#pragma once

/**
 * @file IntervalScan.hpp
 * @brief Runs of a score sequence that stay above a threshold, found in order as a stream, for MayaFlux::Kinesis
 *
 * Domain-agnostic: the scores can be any per-window measurement. Positions are
 * in samples, 32 bit.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @struct IntervalScanConfig
 * @brief Settings of an IntervalScanner
 *
 * Score i of a stream sits at sample first_sample + i * hop. A run starts at a
 * score at or above enter and is released by a score below leave once the quiet
 * has lasted min_gap. A run not longer than min_run is dropped. Each interval
 * is widened by padding on both sides, and a gap between neighbours smaller than
 * twice the padding is split between them.
 *
 * max_run caps one interval, 0 for no cap, and must exceed hop + 2 * padding
 * when set. A capped run is cut at the longest gap seen inside it when
 * use_longest_gap is set, else at the last gap longer than gap_at_limit, else
 * where the cap falls.
 *
 * With a gate, a position whose gate score is below gate_threshold is silence:
 * an open run ends there at once whatever min_gap is, the position is never
 * engaged, and a gap is bridged only while the gate stays at or above its
 * threshold. A run ended by the gate closes where its last engaged stretch
 * ended, before padding.
 *
 * length is the stream length used to clamp padded ends, 0 for the end of the
 * scores seen so far.
 *
 * With emit_on_close an interval is released the moment it closes, padded by
 * the full padding on both sides. Nothing waits for a successor, so latency is
 * the trailing gap alone, and the gap between neighbours is no longer split:
 * their paddings may overlap.
 */
struct IntervalScanConfig {
    double enter { 0.5 };
    double leave { 0.35 };
    double gate_threshold { 0.5 };
    uint32_t hop { 512 };
    uint32_t first_sample { 0 };
    uint32_t min_run { 4000 };
    uint32_t min_gap { 1600 };
    uint32_t padding { 480 };
    uint32_t gap_at_limit { 1568 };
    uint32_t max_run { 0 };
    bool use_longest_gap { true };
    uint32_t length { 0 };
    bool emit_on_close { false };
};

/**
 * @brief Why a config cannot be scanned.
 * @return The reason, empty when the config is valid: thresholds finite, hop above 0, and max_run, when set, above hop plus twice the padding.
 */
[[nodiscard]] MAYAFLUX_API std::string diagnose_interval_scan(const IntervalScanConfig& config);

/**
 * @class IntervalScanner
 * @brief The two threshold run detector as a stateful object
 *
 * Every score depends on the state the previous one left, so scores are read in
 * order, and that state persists between calls: successive push calls continue
 * one stream. The newest interval stays pending until its successor closes,
 * because its padded end depends on the gap that follows; flush releases it and
 * ends the stream. Scores are compared as 32 bit floats, so the intervals match
 * those of the GPU IntervalExtractor for the same scores and settings.
 */
class MAYAFLUX_API IntervalScanner {
public:
    using Interval = std::pair<size_t, size_t>;

    /**
     * @brief Construct a scanner
     * @throws std::invalid_argument When diagnose_interval_scan gives a reason
     */
    explicit IntervalScanner(const IntervalScanConfig& config);

    /**
     * @brief Continue the stream with more scores
     * @param scores Scores in order
     * @param gate One gate score per score, or empty for no gate
     * @return The half open intervals completed by this call, in order
     * @throws std::invalid_argument When gate is not empty and its length differs from the scores
     */
    [[nodiscard]] std::vector<Interval> push(std::span<const double> scores, std::span<const double> gate = {});

    /**
     * @brief End the stream: close the open run and release the pending interval
     * @return The intervals this releases. The next push starts a new stream.
     */
    [[nodiscard]] std::vector<Interval> flush();

    /** @brief Start a new stream without releasing anything. */
    void reset() noexcept;

    [[nodiscard]] const IntervalScanConfig& config() const noexcept { return m_config; }

    /** @brief Start of the run in progress after the last score, if one is open. */
    [[nodiscard]] std::optional<size_t> open_start() const noexcept
    {
        return m_has_start ? std::optional<size_t> { m_start } : std::nullopt;
    }

    /** @brief The closed interval still waiting for its padded end: its padded start and its end before trailing padding. */
    [[nodiscard]] std::optional<Interval> pending() const noexcept
    {
        return m_pend_valid ? std::optional<Interval> { Interval { m_pend_start, m_pend_end } } : std::nullopt;
    }

private:
    IntervalScanConfig m_config;
    uint32_t m_run_limit {};
    uint32_t m_total {};
    uint64_t m_seen {};

    bool m_triggered {};
    uint32_t m_start {};
    bool m_has_start {};
    uint32_t m_temp_end {};
    uint32_t m_prev_end {};
    uint32_t m_next_start {};
    uint32_t m_best_end {};
    uint32_t m_best_dur {};
    bool m_has_best {};
    bool m_pend_valid {};
    uint32_t m_pend_start {};
    uint32_t m_pend_end {};

    void close_run(uint32_t start, uint32_t end, std::vector<Interval>& out);
    void reset_gap_state() noexcept;
    void update_total() noexcept;
};

} // namespace MayaFlux::Kinesis::Discrete
