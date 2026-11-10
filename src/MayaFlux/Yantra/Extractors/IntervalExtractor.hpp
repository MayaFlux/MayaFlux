#pragma once

#include "Extractor.hpp"

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/Executors/Audio/IntervalScanStage.hpp"
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
 * @struct IntervalBuffers
 * @brief Device views of one channel's results from the latest call, for a consumer that reads them on the GPU.
 *
 * runs holds start and end pairs and report holds interval_scan_report_words
 * words that decode_interval_scan_report reads, all uint32. The addresses are
 * device addresses for push constants. The extractor owns the
 * memory and rewrites it on its next call, so a consumer recorded between two
 * calls, in a submission made after the extractor's own, reads the latest
 * results: submissions on one queue run in order. Addresses are 0 for a
 * channel the call gave no scores.
 */
struct MAYAFLUX_API IntervalBuffers {
    uint64_t runs_address { 0 };
    uint64_t report_address { 0 };
    /** Pairs the run list can hold. */
    uint32_t capacity { 0 };
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> runs;
    std::shared_ptr<Portal::Graphics::GpuBufferHandle> report;
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
     * @brief Record and submit an extraction without waiting for it.
     *
     * For a live caller that must not block: the work is on the queue when this
     * returns. Poll ready, then collect the result. Only one submission can be
     * outstanding, and a synchronous extract_intervals is refused while one is.
     *
     * @return False when an earlier submission has not been collected, or the
     *         input could not be submitted.
     */
    bool submit(const input_type& data)
    {
        if (m_outstanding) {
            return false;
        }
        try {
            const auto& cfg = this->template config<IntervalExtractorConfig>();
            [[maybe_unused]] auto [numeric_data, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(data));
            auto calls = record_all(numeric_data, cfg);
            m_outstanding = Outstanding { .calls = std::move(calls), .config = cfg, .fence = m_recording->submit() };
            return true;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Interval submit failed: {}", e.what());
            return false;
        }
    }

    bool submit(const InputType& data)
    {
        return this->submit(input_type { data });
    }

    /**
     * @brief True while a submission is outstanding and not yet collected.
     */
    [[nodiscard]] bool pending() const
    {
        return m_outstanding.has_value();
    }

    /**
     * @brief True when nothing is outstanding or the outstanding submission has completed. Never waits.
     */
    [[nodiscard]] bool ready() const
    {
        return !m_outstanding || m_recording->is_complete(m_outstanding->fence);
    }

    /**
     * @brief The result of the outstanding submission, once it has completed.
     * @return The record, or nothing when none is outstanding or it has not completed yet. Never waits.
     */
    [[nodiscard]] std::optional<IntervalExtraction> collect()
    {
        if (!m_outstanding || !ready()) {
            return std::nullopt;
        }
        IntervalExtraction record;
        record.config = m_outstanding->config;
        record.channels.resize(m_outstanding->calls.size());
        for (size_t channel = 0; channel < m_outstanding->calls.size(); ++channel) {
            record.channels[channel] = read_channel(channel, m_outstanding->calls[channel]);
        }
        m_outstanding.reset();
        return record;
    }

    /**
     * @brief Device views of the results of the latest call, one per channel.
     *
     * Valid as soon as the call has recorded, so after submit they can be
     * handed to a consumer before the work completes. Empty before any call.
     */
    [[nodiscard]] std::vector<IntervalBuffers> buffers() const
    {
        std::vector<IntervalBuffers> views(m_last_calls.size());
        for (size_t channel = 0; channel < m_last_calls.size(); ++channel) {
            if (!m_last_calls[channel].ran) {
                continue;
            }
            const size_t runs = binding_of(channel, Runs);
            const size_t report = binding_of(channel, Report);
            views[channel] = {
                .runs_address = m_context->shared_buffer_address(0, runs),
                .report_address = m_context->shared_buffer_address(0, report),
                .capacity = m_last_calls[channel].capacity,
                .runs = std::make_shared<Portal::Graphics::GpuBufferHandle>(m_context->shared_buffer_handle(0, runs)),
                .report = std::make_shared<Portal::Graphics::GpuBufferHandle>(m_context->shared_buffer_handle(0, report)),
            };
        }
        return views;
    }

    /**
     * @brief Start a new stream: forget the detector state of every channel.
     *
     * Waits for an outstanding submission and discards its result.
     */
    void reset()
    {
        if (m_recording) {
            m_recording->wait();
        }
        m_outstanding.reset();
        m_last_calls.clear();
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

            if (m_outstanding) {
                error<std::logic_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "An earlier submit has not been collected");
            }
            const auto calls = record_all(numeric_data, cfg);

            IntervalExtraction record;
            record.config = cfg;
            record.channels.resize(numeric_data.size());

            std::vector<std::vector<double>> flat(numeric_data.size());
            for (size_t channel = 0; channel < numeric_data.size(); ++channel) {
                record.channels[channel] = read_channel(channel, calls[channel]);
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
        Report,
        State,
        SlotsPerChannel
    };

    static constexpr size_t k_gate_binding = 0;

    struct ChannelCall {
        bool ran { false };
        uint32_t capacity { 0 };
    };

    struct Outstanding {
        std::vector<ChannelCall> calls;
        IntervalExtractorConfig config;
        Portal::Graphics::FenceID fence { Portal::Graphics::INVALID_FENCE };
    };

    [[nodiscard]] static size_t binding_of(size_t channel, Slot slot)
    {
        return 1 + channel * SlotsPerChannel + slot;
    }

    mutable std::shared_ptr<GpuDispatchRecorder> m_recording;
    mutable std::unique_ptr<ShaderExecutionContext<>> m_context;
    std::vector<uint32_t> m_seen;
    std::vector<bool> m_started;
    std::vector<ChannelCall> m_last_calls;
    std::optional<Outstanding> m_outstanding;

    template <typename Columns>
    std::vector<ChannelCall> record_all(const Columns& columns, const IntervalExtractorConfig& cfg)
    {
        ensure_context();
        if (m_seen.size() != columns.size()) {
            m_seen.assign(columns.size(), 0);
            m_started.assign(columns.size(), false);
        }

        const bool gated = cfg.gate_channel != IntervalExtractorConfig::no_channel;
        if (gated && cfg.gate_channel >= columns.size()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "Gate channel {} is not a column of the {} given", cfg.gate_channel, columns.size());
        }
        std::span<const double> gate;
        if (gated) {
            gate = { columns[cfg.gate_channel].data(), columns[cfg.gate_channel].size() };
            upload_gate(gate);
        }

        const auto scan = scan_config(cfg);
        std::vector<DependencyStage> stages;
        std::vector<ChannelCall> calls(columns.size());
        for (size_t channel = 0; channel < columns.size(); ++channel) {
            const std::span<const double> scores { columns[channel].data(), columns[channel].size() };
            const bool gates_this = gated && channel != cfg.gate_channel;
            if (gates_this && gate.size() != scores.size()) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "Gate column has {} scores and column {} has {}", gate.size(), channel, scores.size());
            }
            calls[channel] = record_channel(channel, scores, gates_this, scan, cfg.flush, stages);
        }
        if (!stages.empty()) {
            m_context->record_stages(stages);
        }
        m_last_calls = calls;
        return calls;
    }

    static void validate_config(const IntervalExtractorConfig& cfg)
    {
        const auto fail = [](std::string_view message) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}", message);
        };
        if (!std::isfinite(cfg.leave_margin) || !std::isfinite(cfg.leave_floor)) {
            fail("Thresholds must be finite");
        }
        if (const std::string reason = Kinesis::Discrete::diagnose_interval_scan(scan_config(cfg)); !reason.empty()) {
            fail(reason);
        }
    }

    static Kinesis::Discrete::IntervalScanConfig scan_config(const IntervalExtractorConfig& cfg)
    {
        return {
            .enter = cfg.enter_threshold,
            .leave = std::max(cfg.enter_threshold - cfg.leave_margin, cfg.leave_floor),
            .gate_threshold = cfg.gate_threshold,
            .hop = cfg.samples_per_score,
            .first_sample = cfg.first_sample,
            .min_run = cfg.min_run,
            .min_gap = cfg.min_gap,
            .padding = cfg.padding,
            .gap_at_limit = cfg.gap_at_limit,
            .max_run = cfg.max_run,
            .use_longest_gap = cfg.use_longest_gap,
            .length = cfg.length,
            .emit_on_close = cfg.emit_on_close,
        };
    }

    void ensure_context() const
    {
        if (m_context) {
            return;
        }
        m_recording = std::make_shared<GpuDispatchRecorder>(true);
        m_context = std::make_unique<ShaderExecutionContext<>>(
            GpuComputeConfig { .shader_path = interval_scan_shader, .workgroup_size = { 1, 1, 1 }, .push_constant_size = sizeof(IntervalScanPC) });
        m_context->set_recording(m_recording);
        if (!m_context->ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "IntervalExtractor needs a graphics device and none is available");
        }
    }

    void upload_gate(std::span<const double> gate) const
    {
        constexpr auto type = GpuBufferBinding::ElementType::FLOAT32;
        m_context->ensure_shared_buffer(0, k_gate_binding, std::max<size_t>(gate.size(), 1), type, Portal::Graphics::BufferUsageHint::COMPUTE);
        if (!gate.empty()) {
            const std::vector<float> floats(gate.begin(), gate.end());
            m_context->upload_shared_raw(0, k_gate_binding, reinterpret_cast<const uint8_t*>(floats.data()), floats.size() * sizeof(float));
        }
    }

    ChannelCall record_channel(size_t channel, std::span<const double> scores, bool gated,
        const Kinesis::Discrete::IntervalScanConfig& scan, bool flush, std::vector<DependencyStage>& stages)
    {
        if (scores.empty() && !flush) {
            return {};
        }

        using Portal::Graphics::BufferUsageHint;
        constexpr auto word = GpuBufferBinding::ElementType::UINT32;
        const auto n = static_cast<uint32_t>(scores.size());
        const size_t scores_binding = binding_of(channel, Scores);
        const size_t runs_binding = binding_of(channel, Runs);
        const size_t report_binding = binding_of(channel, Report);
        const size_t state_binding = binding_of(channel, State);

        m_context->ensure_shared_buffer(0, scores_binding, std::max<size_t>(n, 1), GpuBufferBinding::ElementType::FLOAT32, BufferUsageHint::COMPUTE);
        m_context->ensure_shared_buffer(0, runs_binding, interval_scan_runs_words(n), word);
        m_context->ensure_shared_buffer(0, report_binding, interval_scan_report_words, word);
        m_context->ensure_shared_buffer(0, state_binding, interval_scan_state_words, word, BufferUsageHint::COMPUTE);

        if (!m_started[channel]) {
            const std::vector<uint32_t> zeros(interval_scan_state_words, 0U);
            m_context->upload_shared_raw(0, state_binding, reinterpret_cast<const uint8_t*>(zeros.data()), zeros.size() * sizeof(uint32_t));
            m_started[channel] = true;
        }
        if (n > 0) {
            const std::vector<float> floats(scores.begin(), scores.end());
            m_context->upload_shared_raw(0, scores_binding, reinterpret_cast<const uint8_t*>(floats.data()), floats.size() * sizeof(float));
        }
        const uint32_t none = 0;
        m_context->upload_shared_raw(0, report_binding, reinterpret_cast<const uint8_t*>(&none), sizeof(none));

        const uint32_t first = m_seen[channel];
        stages.push_back(interval_scan_stage(scan,
            m_context->shared_buffer_address(0, scores_binding),
            gated ? m_context->shared_buffer_address(0, k_gate_binding) : 0U,
            m_context->shared_buffer_address(0, state_binding),
            m_context->shared_buffer_address(0, runs_binding),
            m_context->shared_buffer_address(0, report_binding),
            first, n, flush));

        if (flush) {
            m_seen[channel] = 0;
            m_started[channel] = false;
        } else {
            m_seen[channel] = first + n;
        }
        return { .ran = true, .capacity = n + 4 };
    }

    ChannelIntervals read_channel(size_t channel, const ChannelCall& call) const
    {
        ChannelIntervals result;
        if (!call.ran) {
            return result;
        }

        std::array<uint32_t, interval_scan_report_words> words {};
        m_context->download_shared(0, binding_of(channel, Report), words.data(), sizeof(words));
        const auto report = decode_interval_scan_report(words);

        if (report.emitted > 0) {
            std::vector<uint32_t> runs(static_cast<size_t>(report.emitted) * 2);
            m_context->download_shared(0, binding_of(channel, Runs), runs.data(), runs.size() * sizeof(uint32_t));
            result.intervals.reserve(report.emitted);
            for (uint32_t i = 0; i < report.emitted; ++i) {
                result.intervals.emplace_back(runs[2 * i], runs[2 * i + 1]);
            }
        }
        result.open_start = report.open_start;
        result.pending = report.pending;
        return result;
    }
};

/// Matrix interval extractor: the MatrixXd an analyzer returns, one score channel per column -> interval pairs per channel
using MatrixIntervalExtractor = IntervalExtractor<Eigen::MatrixXd, std::vector<std::vector<double>>>;

/// Standard interval extractor: DataVariant scores -> interval pairs per channel
using StandardIntervalExtractor = IntervalExtractor<std::vector<Kakshya::DataVariant>, std::vector<std::vector<double>>>;

} // namespace MayaFlux::Yantra
