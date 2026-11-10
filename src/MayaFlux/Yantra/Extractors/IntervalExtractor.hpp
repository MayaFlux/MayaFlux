#pragma once

#include "Extractor.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file IntervalExtractor.hpp
 * @brief Runs of a score sequence that stay above a threshold, found on the GPU in order.
 *
 * The scores can come from any per-window measurement: a probability, a level,
 * a flux. The extractor knows nothing about what they mean. It answers where
 * the sequence is engaged, as sample intervals that line up with the output of
 * merge_intervals and slice_intervals.
 */

namespace MayaFlux::Yantra {

/**
 * @enum IntervalMethod
 * @brief How the score sequence is turned into intervals.
 */
enum class IntervalMethod : uint8_t {
    HYSTERESIS_RUNS ///< Engage at or above the enter threshold, release below the leave threshold after a minimum gap
};

/**
 * @struct IntervalExtractorConfig
 * @brief Settings of an IntervalExtractor.
 *
 * Lengths are in samples and positions are 32 bit, so a stream is limited to
 * about 4.29 billion samples. Score i of a stream sits at sample
 * first_sample + i * samples_per_score. The leave threshold is max(enter_threshold -
 * leave_margin, leave_floor). A run shorter than min_run is dropped. A gap
 * shorter than min_gap does not end a run. Each interval is widened by padding
 * on both sides, and a gap between neighbours smaller than twice the padding
 * is split between them.
 *
 * max_run caps the length of one interval, 0 for no cap. A capped run is cut
 * at the longest gap seen inside it when use_longest_gap is set, else at the
 * last gap longer than gap_at_limit, else where the cap falls. It must exceed
 * samples_per_score + 2 * padding when set.
 *
 * flush ends the stream: the open run is closed at length, or at the end of
 * the scores when length is 0, and the pending interval is emitted. Without
 * flush the newest interval stays pending until the next call.
 *
 * gate_channel names the column that gates all the others, or no_channel for
 * none. Where its score is below gate_threshold a position is silence: an open
 * run of another column ends there at once whatever min_gap is, the position is
 * never engaged, and a gap is bridged only while the gate stays at or above its
 * threshold. The gate column itself is scanned as it is. Padding is applied
 * after a gate break as everywhere else.
 *
 * emit_on_close releases each interval the moment it closes, when its trailing
 * gap reaches min_gap, padded by the full padding on both sides. Nothing waits
 * for a successor, so the gap between neighbours is not split and their
 * paddings may overlap. A padded end is clamped to length, or to the end of the
 * scores seen so far when length is 0.
 */
struct IntervalExtractorConfig : ExtractorConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            ExtractorConfig::describe_facets(),
            std::tuple {
                FacetMember<&IntervalExtractorConfig::method> {},
                FacetMember<&IntervalExtractorConfig::enter_threshold> {},
                FacetMember<&IntervalExtractorConfig::leave_margin> {},
                FacetMember<&IntervalExtractorConfig::leave_floor> {},
                FacetMember<&IntervalExtractorConfig::samples_per_score> {},
                FacetMember<&IntervalExtractorConfig::min_run> {},
                FacetMember<&IntervalExtractorConfig::min_gap> {},
                FacetMember<&IntervalExtractorConfig::padding> {},
                FacetMember<&IntervalExtractorConfig::gap_at_limit> {},
                FacetMember<&IntervalExtractorConfig::max_run> {},
                FacetMember<&IntervalExtractorConfig::use_longest_gap> {},
                FacetMember<&IntervalExtractorConfig::flush> {},
                FacetMember<&IntervalExtractorConfig::length> {},
                FacetMember<&IntervalExtractorConfig::first_sample> {},
                FacetMember<&IntervalExtractorConfig::gate_channel> {},
                FacetMember<&IntervalExtractorConfig::gate_threshold> {},
                FacetMember<&IntervalExtractorConfig::emit_on_close> {},
            });
    }

    /** Value of gate_channel for no gate. */
    static constexpr uint32_t no_channel { std::numeric_limits<uint32_t>::max() };

    /** Selection method, as an IntervalMethod value. */
    Reflect::EnumByte<IntervalMethod> method { IntervalMethod::HYSTERESIS_RUNS };
    /** Score at or above which a run starts. */
    double enter_threshold { 0.5 };
    /** How far below the enter threshold a score must fall to count as a gap. */
    double leave_margin { 0.15 };
    /** Lowest leave threshold. */
    double leave_floor { 0.01 };
    /** Samples between successive scores. */
    uint32_t samples_per_score { 512 };
    /** Shortest run kept, in samples. */
    uint32_t min_run { 4000 };
    /** Shortest gap that ends a run, in samples. */
    uint32_t min_gap { 1600 };
    /** Samples added on each side of an interval. */
    uint32_t padding { 480 };
    /** Gaps longer than this are candidates for cutting a capped run, in samples. */
    uint32_t gap_at_limit { 1568 };
    /** Longest interval in samples, 0 for no cap. */
    uint32_t max_run { 0 };
    /** Cut a capped run at its longest gap instead of the last one. */
    bool use_longest_gap { true };
    /** End the stream with this call. */
    bool flush { false };
    /** Length of the stream in samples when it ends, 0 to use the scores seen. */
    uint32_t length { 0 };
    /** Sample position of score 0. */
    uint32_t first_sample { 0 };
    /** Column that gates the others, or no_channel. */
    uint32_t gate_channel { no_channel };
    /** Gate score below which a position is silence. */
    double gate_threshold { 0.5 };
    /** Emit each interval as soon as it closes, with full padding on both sides. */
    bool emit_on_close { false };
};

/**
 * @struct ChannelIntervals
 * @brief Intervals of one channel, half open sample ranges in stream order.
 *
 * open_start and pending describe the state after the last score of the call,
 * so a live caller need not wait for a run to close. open_start is where the
 * run in progress began, set from the first score at or above the enter
 * threshold until the run closes, and so also during a gap not yet long enough
 * to end it. pending is the closed interval waiting for its padded end: its
 * padded start and its end before trailing padding. Both are empty after a
 * flush, and with emit_on_close nothing is ever pending. A call that gives a
 * channel no scores and does not flush reports neither.
 */
struct MAYAFLUX_API ChannelIntervals {
    std::vector<std::pair<size_t, size_t>> intervals;
    std::optional<size_t> open_start;
    std::optional<std::pair<size_t, size_t>> pending;
};

/**
 * @struct IntervalExtraction
 * @brief Record of one IntervalExtractor run, stored on the output's facets.
 */
struct MAYAFLUX_API IntervalExtraction {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&IntervalExtraction::config> {},
            FacetMember<&IntervalExtraction::channels> {},
        };
    }

    /** Settings the extraction was produced under. */
    IntervalExtractorConfig config;
    /** Intervals completed by this call, per score channel. */
    std::vector<ChannelIntervals> channels;
};

/**
 * @brief One region per interval, spanning the given channels.
 * @param intervals Half open sample ranges. Empty ranges are skipped.
 * @param first_channel First channel the regions cover.
 * @param last_channel Last channel the regions cover, inclusive.
 * @param name Name of the group.
 */
[[nodiscard]] inline Kakshya::RegionGroup interval_regions(
    const ChannelIntervals& intervals, uint32_t first_channel, uint32_t last_channel, std::string name = "intervals")
{
    Kakshya::RegionGroup group(std::move(name));
    for (const auto& [start, end] : intervals.intervals) {
        if (end > start) {
            group.add_region(Kakshya::Region::audio_span(start, end - 1, first_channel, last_channel));
        }
    }
    return group;
}

/**
 * @class IntervalExtractor
 * @brief Finds where a score sequence is engaged, on the GPU, as a stream.
 *
 * Every score depends on the state the previous one left, so one invocation
 * reads them in order. That state lives on the device and persists between
 * calls, so successive calls continue one stream. The newest interval waits in
 * the state until its successor closes, because its padded end depends on the
 * gap that follows; set flush on the last call to release it. A flush ends the
 * stream and the next call starts a new one. Call reset to start over without
 * flushing.
 *
 * The input is the matrix an analyzer returns, one score channel per column,
 * so an analyzer such as VadAnalyzer chains into it directly. Other input
 * types are instantiations of the same template.
 *
 * It is on the vulkan target. Kinesis::Discrete::IntervalScanner is its CPU
 * counterpart for scores in host memory and gives the same intervals. The output data is, per
 * channel, the start and end of each completed interval in turn; the
 * IntervalExtraction facet holds the same as pairs. Each channel of the input
 * is a separate score sequence with its own state.
 */
template <ComputeData InputType = Eigen::MatrixXd, ComputeData OutputType = std::vector<std::vector<double>>>
class IntervalExtractor : public Extractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Extractor<InputType, OutputType>;
    using supports = Supports<IntervalExtractorConfig>;

    static constexpr Target target = targets::vulkan;
    static constexpr bool stateful = true;

    /**
     * @brief Construct an interval extractor from its settings.
     * @param config Extraction settings, validated before they are stored.
     */
    explicit IntervalExtractor(IntervalExtractorConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::PATTERN_BASED;
    }

    /**
     * @brief Type-safe interval extraction
     * @return The intervals completed by this call.
     */
    IntervalExtraction extract_intervals(const input_type& data)
    {
        const auto result = this->apply_operation(data);
        const auto* record = result.facets.template get<IntervalExtraction>();
        if (record == nullptr) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "Interval extraction produced no result");
        }
        return *record;
    }

    IntervalExtraction extract_intervals(const InputType& data)
    {
        return this->extract_intervals(input_type { data });
    }

    /**
     * @brief Start a new stream: forget the detector state of every channel.
     */
    void reset()
    {
        m_seen.clear();
        m_started.clear();
    }

    /**
     * @brief One while a device is up, zero otherwise.
     */
    static float fitness(const input_type& /*input*/)
    {
        return Portal::Graphics::get_shader_foundry().is_initialized() ? 1.0F : 0.0F;
    }

protected:
    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "IntervalExtractor";
    }

    output_type extract_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<IntervalExtractorConfig>();

            auto [numeric_data, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(input));
            DataStructureInfo structure_info = info;

            ensure_context();
            if (m_seen.size() != numeric_data.size()) {
                m_seen.assign(numeric_data.size(), 0);
                m_started.assign(numeric_data.size(), false);
            }

            IntervalExtraction record;
            record.config = cfg;
            record.channels.resize(numeric_data.size());

            const bool gated = cfg.gate_channel != IntervalExtractorConfig::no_channel;
            if (gated && cfg.gate_channel >= numeric_data.size()) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "Gate channel {} is not a column of the {} given", cfg.gate_channel, numeric_data.size());
            }
            std::span<const double> gate;
            if (gated) {
                gate = { numeric_data[cfg.gate_channel].data(), numeric_data[cfg.gate_channel].size() };
            }

            std::vector<std::vector<double>> flat(numeric_data.size());
            for (size_t channel = 0; channel < numeric_data.size(); ++channel) {
                const std::span<const double> scores { numeric_data[channel].data(), numeric_data[channel].size() };
                const bool gates_this = gated && channel != cfg.gate_channel;
                if (gates_this && gate.size() != scores.size()) {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                        std::source_location::current(), "Gate column has {} scores and column {} has {}", gate.size(), channel, scores.size());
                }
                record.channels[channel] = run_channel(channel, scores, gates_this ? gate : std::span<const double> {}, cfg);
                for (const auto& [start, end] : record.channels[channel].intervals) {
                    flat[channel].push_back(static_cast<double>(start));
                    flat[channel].push_back(static_cast<double>(end));
                }
            }

            output_type output = this->convert_result(flat, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(record);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Interval extraction failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    enum Slot : uint32_t {
        Scores,
        Runs,
        Count,
        Gate,
        State
    };

    static constexpr uint32_t k_state_words = 16;
    static constexpr uint32_t k_flag_longest = 1;
    static constexpr uint32_t k_flag_flush = 2;
    static constexpr uint32_t k_flag_gated = 4;
    static constexpr uint32_t k_flag_emit_on_close = 8;
    static constexpr uint32_t k_report_words = 6;

    struct Push {
        uint64_t scores;
        uint64_t state;
        uint64_t runs;
        uint64_t count;
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

    static_assert(sizeof(Push) == 96);

    mutable std::shared_ptr<GpuDispatchRecorder> m_recording;
    mutable std::unique_ptr<ShaderExecutionContext<>> m_context;
    std::vector<uint32_t> m_seen;
    std::vector<bool> m_started;

    static void validate_config(const IntervalExtractorConfig& cfg)
    {
        const auto fail = [](std::string_view message) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}", message);
        };
        if (!std::isfinite(cfg.enter_threshold) || !std::isfinite(cfg.leave_margin) || !std::isfinite(cfg.leave_floor)) {
            fail("Thresholds must be finite");
        }
        if (!std::isfinite(cfg.gate_threshold)) {
            fail("The gate threshold must be finite");
        }
        if (cfg.samples_per_score == 0) {
            fail("Samples per score must be greater than 0");
        }
        if (cfg.max_run != 0 && cfg.max_run <= static_cast<uint64_t>(cfg.samples_per_score) + 2ULL * cfg.padding) {
            fail("The run cap must exceed one score plus twice the padding");
        }
    }

    static float ceil_float(double value)
    {
        auto narrowed = static_cast<float>(value);
        if (static_cast<double>(narrowed) < value) {
            narrowed = std::nextafter(narrowed, std::numeric_limits<float>::infinity());
        }
        return narrowed;
    }

    void ensure_context() const
    {
        if (m_context) {
            return;
        }
        m_recording = std::make_shared<GpuDispatchRecorder>(true);
        m_context = std::make_unique<ShaderExecutionContext<>>(
            GpuComputeConfig { .shader_path = "interval_scan.comp.spv", .workgroup_size = { 1, 1, 1 }, .push_constant_size = sizeof(Push) });
        m_context->set_recording(m_recording);
        if (!m_context->ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "IntervalExtractor needs a graphics device and none is available");
        }
    }

    ChannelIntervals run_channel(
        size_t channel, std::span<const double> scores, std::span<const double> gate, const IntervalExtractorConfig& cfg)
    {
        if (scores.empty() && !cfg.flush) {
            return {};
        }

        using Portal::Graphics::BufferUsageHint;
        constexpr auto word = GpuBufferBinding::ElementType::UINT32;
        const auto n = static_cast<uint32_t>(scores.size());
        const auto state_binding = static_cast<size_t>(State + channel);

        std::vector<float> floats(scores.begin(), scores.end());

        m_context->ensure_shared_buffer(0, Scores, std::max<size_t>(n, 1), GpuBufferBinding::ElementType::FLOAT32, BufferUsageHint::COMPUTE);
        m_context->ensure_shared_buffer(0, Runs, (static_cast<size_t>(n) + 4) * 2, word);
        m_context->ensure_shared_buffer(0, Count, k_report_words, word);
        m_context->ensure_shared_buffer(0, Gate, std::max<size_t>(n, 1), GpuBufferBinding::ElementType::FLOAT32, BufferUsageHint::COMPUTE);
        m_context->ensure_shared_buffer(0, state_binding, k_state_words, word, BufferUsageHint::COMPUTE);

        if (!m_started[channel]) {
            const std::vector<uint32_t> zeros(k_state_words, 0U);
            m_context->upload_shared_raw(0, state_binding, reinterpret_cast<const uint8_t*>(zeros.data()), zeros.size() * sizeof(uint32_t));
            m_started[channel] = true;
        }
        if (n > 0) {
            m_context->upload_shared_raw(0, Scores, reinterpret_cast<const uint8_t*>(floats.data()), floats.size() * sizeof(float));
        }
        if (!gate.empty()) {
            const std::vector<float> gate_floats(gate.begin(), gate.end());
            m_context->upload_shared_raw(0, Gate, reinterpret_cast<const uint8_t*>(gate_floats.data()), gate_floats.size() * sizeof(float));
        }
        const uint32_t none = 0;
        m_context->upload_shared_raw(0, Count, reinterpret_cast<const uint8_t*>(&none), sizeof(none));

        const uint32_t first = m_seen[channel];
        const uint64_t seen_end = static_cast<uint64_t>(first) + n;
        const auto span_end = static_cast<uint32_t>(cfg.first_sample + seen_end * cfg.samples_per_score);
        const double leave = std::max(cfg.enter_threshold - cfg.leave_margin, cfg.leave_floor);

        const Push push {
            .scores = m_context->shared_buffer_address(0, Scores),
            .state = m_context->shared_buffer_address(0, state_binding),
            .runs = m_context->shared_buffer_address(0, Runs),
            .count = m_context->shared_buffer_address(0, Count),
            .gate = m_context->shared_buffer_address(0, Gate),
            .n = n,
            .first_index = first,
            .hop = cfg.samples_per_score,
            .min_run = cfg.min_run,
            .min_gap = cfg.min_gap,
            .pad = cfg.padding,
            .gap_at_limit = cfg.gap_at_limit,
            .run_limit = cfg.max_run == 0 ? std::numeric_limits<uint32_t>::max() : cfg.max_run - cfg.samples_per_score - 2 * cfg.padding,
            .flags = (cfg.use_longest_gap ? k_flag_longest : 0U) | (cfg.flush ? k_flag_flush : 0U) | (gate.empty() ? 0U : k_flag_gated) | (cfg.emit_on_close ? k_flag_emit_on_close : 0U),
            .total = cfg.length != 0 ? cfg.length : span_end,
            .first_sample = cfg.first_sample,
            .enter = ceil_float(cfg.enter_threshold),
            .leave = ceil_float(leave),
            .gate_threshold = ceil_float(cfg.gate_threshold),
        };

        m_context->record_stages({ DependencyStage {
            .config = GpuComputeConfig { .shader_path = "interval_scan.comp.spv", .workgroup_size = { 1, 1, 1 }, .push_constant_size = sizeof(Push) },
            .stage_fn = [push](GpuDispatchCore& core) { core.set_push_constants(push); },
            .explicit_groups = std::array<uint32_t, 3> { 1U, 1U, 1U },
        } });

        std::array<uint32_t, k_report_words> report {};
        m_context->download_shared(0, Count, report.data(), sizeof(report));
        const uint32_t emitted = report[0];
        ChannelIntervals result;
        if (emitted > 0) {
            std::vector<uint32_t> words(static_cast<size_t>(emitted) * 2);
            m_context->download_shared(0, Runs, words.data(), words.size() * sizeof(uint32_t));
            result.intervals.reserve(emitted);
            for (uint32_t i = 0; i < emitted; ++i) {
                result.intervals.emplace_back(words[2 * i], words[2 * i + 1]);
            }
        }
        if (report[1] != 0) {
            result.open_start = report[2];
        }
        if (report[3] != 0) {
            result.pending = std::pair<size_t, size_t> { report[4], report[5] };
        }

        if (cfg.flush) {
            m_seen[channel] = 0;
            m_started[channel] = false;
        } else {
            m_seen[channel] = static_cast<uint32_t>(seen_end);
        }
        return result;
    }
};

/// Matrix interval extractor: the MatrixXd an analyzer returns, one score channel per column -> interval pairs per channel
using MatrixIntervalExtractor = IntervalExtractor<Eigen::MatrixXd, std::vector<std::vector<double>>>;

/// Standard interval extractor: DataVariant scores -> interval pairs per channel
using StandardIntervalExtractor = IntervalExtractor<std::vector<Kakshya::DataVariant>, std::vector<std::vector<double>>>;

} // namespace MayaFlux::Yantra
