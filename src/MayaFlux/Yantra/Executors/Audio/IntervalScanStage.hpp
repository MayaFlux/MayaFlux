#pragma once

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Discrete/IntervalScan.hpp"
#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"

/**
 * @file IntervalScanStage.hpp
 * @brief The stage that runs the interval scan on device memory, its push layout and its buffer contract.
 *
 * The scan is the GPU form of Kinesis::Discrete::IntervalScanner and gives the
 * same intervals. The stage reads scores through a device address, so the
 * output buffer of a network recorded in the same submission feeds it with no
 * read-back in between. A layout does not change once released; fields are
 * added only by taking a reserved word.
 */

namespace MayaFlux::Yantra {

inline constexpr const char* interval_scan_shader = "interval_scan.comp.spv";

/** @brief Words of the state buffer. Zero it to start a stream. */
inline constexpr uint32_t interval_scan_state_words = 16;
/** @brief Words of the report buffer: the run count, then the open run and the pending interval. */
inline constexpr uint32_t interval_scan_report_words = 6;

inline constexpr uint32_t interval_scan_flag_longest = 1;
inline constexpr uint32_t interval_scan_flag_flush = 2;
inline constexpr uint32_t interval_scan_flag_gated = 4;
inline constexpr uint32_t interval_scan_flag_emit_on_close = 8;

/** @brief Push constants of interval_scan.comp. */
struct IntervalScanPC {
    uint64_t scores;
    uint64_t state;
    uint64_t runs;
    uint64_t report;
    uint64_t gate;
    uint32_t n;
    uint32_t first_index;
    uint32_t hop;
    uint32_t min_run;
    uint32_t min_gap;
    uint32_t pad;
    uint32_t gap_at_limit;
    uint32_t run_limit;
    uint32_t flags;
    uint32_t total;
    uint32_t first_sample;
    float enter;
    float leave;
    float gate_threshold;
};

static_assert(sizeof(IntervalScanPC) == 96);

/**
 * @brief Words the run list needs for n scores: a start and an end for each run that can close, plus slack.
 */
[[nodiscard]] constexpr size_t interval_scan_runs_words(size_t n) noexcept
{
    return (n + 4) * 2;
}

/**
 * @struct IntervalScanReport
 * @brief What one dispatch of the scan left, decoded from the report buffer.
 */
struct IntervalScanReport {
    /** Intervals this dispatch appended to the run list. */
    uint32_t emitted { 0 };
    /** Where the run in progress began. */
    std::optional<size_t> open_start;
    /** The closed interval waiting for its padded end: its padded start and its end before trailing padding. */
    std::optional<std::pair<size_t, size_t>> pending;
};

/**
 * @brief Decode the report buffer.
 * @param words interval_scan_report_words words read back from the report buffer.
 */
[[nodiscard]] inline IntervalScanReport decode_interval_scan_report(std::span<const uint32_t> words)
{
    IntervalScanReport report;
    if (words.size() < interval_scan_report_words) {
        return report;
    }
    report.emitted = words[0];
    if (words[1] != 0) {
        report.open_start = words[2];
    }
    if (words[3] != 0) {
        report.pending = std::pair<size_t, size_t> { words[4], words[5] };
    }
    return report;
}

namespace detail {

    [[nodiscard]] inline float interval_scan_ceil_float(double value) noexcept
    {
        auto narrowed = static_cast<float>(value);
        if (static_cast<double>(narrowed) < value) {
            narrowed = std::nextafter(narrowed, std::numeric_limits<float>::infinity());
        }
        return narrowed;
    }

}

/**
 * @brief One interval_scan dispatch: continue a stream with n scores from device memory.
 *
 * All buffers are float32 or uint32 and addressed by device address. The
 * thresholds are rounded up to floats, so a float score compares exactly as it
 * does in the host scanner. The state buffer persists between calls and is
 * zeroed to start a stream. The first word of the report buffer counts the runs
 * appended and must be zero before each dispatch; the rest is overwritten.
 *
 * @param config Settings. Its length, or when 0 first_sample plus the scores seen after this call, clamps padded ends.
 * @param scores Device address of the scores.
 * @param gate Device address of the gate scores, one per score, or 0 for no gate.
 * @param state Device address of interval_scan_state_words words.
 * @param runs Device address of interval_scan_runs_words(n) words, start and end pairs.
 * @param report Device address of interval_scan_report_words words.
 * @param first_index Scores already seen in this stream.
 * @param n Scores in this call.
 * @param flush End the stream with this call.
 * @throws std::invalid_argument When the config is not valid.
 */
[[nodiscard]] inline DependencyStage interval_scan_stage(const Kinesis::Discrete::IntervalScanConfig& config,
    uint64_t scores, uint64_t gate, uint64_t state, uint64_t runs, uint64_t report,
    uint32_t first_index, uint32_t n, bool flush)
{
    if (const std::string reason = Kinesis::Discrete::diagnose_interval_scan(config); !reason.empty()) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "{}", reason);
    }
    const auto seen_end = static_cast<uint64_t>(first_index) + n;
    const IntervalScanPC pc {
        .scores = scores,
        .state = state,
        .runs = runs,
        .report = report,
        .gate = gate,
        .n = n,
        .first_index = first_index,
        .hop = config.hop,
        .min_run = config.min_run,
        .min_gap = config.min_gap,
        .pad = config.padding,
        .gap_at_limit = config.gap_at_limit,
        .run_limit = config.max_run == 0 ? std::numeric_limits<uint32_t>::max() : config.max_run - config.hop - 2 * config.padding,
        .flags = (config.use_longest_gap ? interval_scan_flag_longest : 0U)
            | (flush ? interval_scan_flag_flush : 0U)
            | (gate != 0 ? interval_scan_flag_gated : 0U)
            | (config.emit_on_close ? interval_scan_flag_emit_on_close : 0U),
        .total = config.length != 0 ? config.length : static_cast<uint32_t>(config.first_sample + seen_end * config.hop),
        .first_sample = config.first_sample,
        .enter = detail::interval_scan_ceil_float(config.enter),
        .leave = detail::interval_scan_ceil_float(config.leave),
        .gate_threshold = detail::interval_scan_ceil_float(config.gate_threshold),
    };
    return DependencyStage {
        .config = GpuComputeConfig { .shader_path = interval_scan_shader, .workgroup_size = { 1, 1, 1 }, .push_constant_size = sizeof(IntervalScanPC) },
        .stage_fn = [pc](GpuDispatchCore& core) { core.set_push_constants(pc); },
        .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
    };
}

} // namespace MayaFlux::Yantra
