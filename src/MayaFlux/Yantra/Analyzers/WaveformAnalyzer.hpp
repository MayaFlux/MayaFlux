#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Kakshya/Source/TensorContainer.hpp"
#include "MayaFlux/Yantra/Executors/Audio/LaggedPair.hpp"
#include "MayaFlux/Yantra/Executors/Audio/SegmentedReductionContext.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file WaveformAnalyzer.hpp
 * @brief Time-domain descriptors of a waveform computed on the GPU, exactly, with no fp64 on the device.
 *
 * The time-domain counterpart of SpectralAnalyzer. Every window of every
 * channel is measured by one submission of several device passes, and only a
 * few doubles per window come back, or a lag profile per window when asked.
 */

namespace MayaFlux::Yantra {

/**
 * @enum WaveformMethod
 * @brief The per-window value a WaveformAnalyzer reports as its output data.
 */
enum class WaveformMethod : uint8_t {
    LEVEL, ///< Root mean square of the window
    CROSSING_RATE, ///< Sign changes between neighbours per sample pair, 0 to 1
    PERIODICITY, ///< Strength of the best repetition lag in the searched range, as a normalised correlation
    PERIOD, ///< Best repetition lag in samples, zero when no lag qualified
    FREQUENCY, ///< Sample rate over the best lag in Hz, zero when no lag qualified
    PEAK, ///< Largest absolute sample
    DC_OFFSET, ///< Mean sample
    CREST_FACTOR, ///< Peak over level, zero for a silent window
    HARMONICITY, ///< Periodicity over one minus periodicity, zero when no lag qualified
    LEVEL_CHANGE, ///< Level minus the previous window's level, zero for the first window
    PERIOD_CHANGE ///< Relative change of the period since the previous window, zero unless both have one
};

/**
 * @struct WaveformAnalyzerConfig
 * @brief Settings of a WaveformAnalyzer.
 *
 * The repetition search covers periods from sample_rate / high_frequency to
 * sample_rate / low_frequency samples, truncated to whole samples. The window
 * must hold at least twice the longest period. The hop must divide 512.
 */
struct WaveformAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&WaveformAnalyzerConfig::method> {},
                FacetMember<&WaveformAnalyzerConfig::sample_rate> {},
                FacetMember<&WaveformAnalyzerConfig::low_frequency> {},
                FacetMember<&WaveformAnalyzerConfig::high_frequency> {},
                FacetMember<&WaveformAnalyzerConfig::keep_profiles> {},
            });
    }

    WaveformAnalyzerConfig()
    {
        window_size = 512;
        hop_size = 256;
    }

    /** Value reported as the output data, as a WaveformMethod value. */
    Reflect::EnumByte<WaveformMethod> method { WaveformMethod::LEVEL };
    /** Samples per second of the analysed signal. */
    double sample_rate { 16000.0 };
    /** Lowest repetition frequency searched, in Hz. */
    double low_frequency { 70.0 };
    /** Highest repetition frequency searched, in Hz. */
    double high_frequency { 400.0 };
    /** Keep the autocorrelation and mean difference profile of every window. They are large: one row of lags per window. */
    bool keep_profiles { false };
};

/**
 * @struct LagProfile
 * @brief One row of values per window, one column per consecutive lag.
 */
struct MAYAFLUX_API LagProfile {
    /** Window major: window w, column c is values[w * lag_count + c]. Empty when profiles are not kept. */
    std::vector<double> values;
    /** Lag of the first column, in samples. */
    uint32_t first_lag { 0 };
    /** Columns per window. */
    uint32_t lag_count { 0 };

    /**
     * @brief The profile as a windows by lags float64 tensor.
     * @param name Name of the tensor.
     * @return The container, or nullptr when the profile holds nothing.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TensorContainer> to_tensor(std::string name) const
    {
        if (values.empty() || lag_count == 0) {
            return nullptr;
        }
        auto data = Kakshya::TensorData::from_values<double>(values, { values.size() / lag_count, lag_count }, std::move(name));
        return data ? std::make_shared<Kakshya::TensorContainer>(std::move(*data)) : nullptr;
    }
};

/**
 * @struct ChannelWaveform
 * @brief Waveform descriptors of one channel, one entry per window.
 */
struct MAYAFLUX_API ChannelWaveform {
    /** The value selected by the method. */
    std::vector<double> values;
    /** Root mean square. */
    std::vector<double> level;
    /** Sign changes per sample pair. */
    std::vector<double> crossing_rate;
    /** Largest absolute sample. */
    std::vector<double> peak;
    /** Mean sample. */
    std::vector<double> dc_offset;
    /** Peak over level, zero for a silent window. */
    std::vector<double> crest_factor;
    /** Normalised correlation at the best lag, zero when none qualified. */
    std::vector<double> periodicity;
    /** Best lag in samples, zero when none qualified. */
    std::vector<double> period;
    /** Sample rate over the best lag in Hz, zero when none qualified. */
    std::vector<double> frequency;
    /** Periodicity over one minus periodicity, finite, zero when none qualified. */
    std::vector<double> harmonicity;
    /** Level minus the previous window's level, zero for the first window. */
    std::vector<double> level_change;
    /** Relative change of the period since the previous window, zero unless both windows have one. */
    std::vector<double> period_change;
    /** First and one past the last sample of each window. */
    std::vector<std::pair<size_t, size_t>> window_positions;
    /** Autocorrelation of the mean-removed window divided by its lag 0 value, lags 0 to the longest searched. Kept on request. */
    LagProfile autocorrelation;
    /** Mean absolute difference between samples a lag apart, lags over the searched range. Kept on request. */
    LagProfile mean_difference;
};

/**
 * @struct WaveformAnalysis
 * @brief Analysis result of a WaveformAnalyzer.
 */
struct MAYAFLUX_API WaveformAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&WaveformAnalysis::config> {},
            FacetMember<&WaveformAnalysis::channels> {},
        };
    }

    /** Settings the analysis was produced under. */
    WaveformAnalyzerConfig config;
    std::vector<ChannelWaveform> channels;
};

/**
 * @class WaveformAnalyzer
 * @brief Time-domain descriptors of every channel, run on the GPU.
 *
 * Measures the shape of the waveform: its level and extremes, how often it
 * changes sign, and at what lag, and how strongly, it repeats. It makes no
 * claim about what the signal is. A sung note, a bowed string and a fan hum
 * all repeat, so deciding that a sound is voice or speech is left to
 * operations built on top of this one.
 *
 * Six device passes (energy, sum, peak, sign changes, centred correlation and
 * optionally mean difference) are recorded into one command buffer and
 * submitted once per chunk of windows. The method picks which descriptor
 * becomes the output data; all of them are always in the analysis. Sums run
 * in emulated double arithmetic on word pairs, so results agree with the
 * Kinesis references (Discrete::rms, mean, peak, zero_crossing_rate and
 * periodicity) to rounding and the device needs no float64. The profiles are
 * the full autocorrelation and mean difference rows a model can read, not
 * only the lag they reduce to; to_tensor hands them over as a tensor.
 * A channel shorter than one window gives no windows.
 *
 * The set of descriptors is a starting point and is expected to grow. It is a
 * distinct operation on the vulkan target and has no CPU twin.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class WaveformAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<WaveformAnalyzerConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct a waveform analyzer from its settings.
     * @param config Analysis settings, validated before they are stored.
     */
    explicit WaveformAnalyzer(WaveformAnalyzerConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Type-safe waveform analysis
     */
    WaveformAnalysis analyze_waveform(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<WaveformAnalysis>(result);
    }

    WaveformAnalysis analyze_waveform(const InputType& data)
    {
        return this->analyze_waveform(input_type { data });
    }

    [[nodiscard]] WaveformAnalysis get_waveform_analysis() const
    {
        return safe_any_cast_or_throw<WaveformAnalysis>(this->get_current_analysis());
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::TEMPORAL;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "WaveformAnalyzer";
    }

    /**
     * @brief One while a device is up, zero otherwise.
     */
    static float fitness(const input_type& /*input*/)
    {
        return Portal::Graphics::get_shader_foundry().is_initialized() ? 1.0F : 0.0F;
    }

protected:
    output_type analyze_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<WaveformAnalyzerConfig>();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            ensure_contexts();
            const uint32_t min_lag = shortest_lag(cfg);
            const uint32_t max_lag = longest_lag(cfg);

            WaveformAnalysis analysis;
            analysis.config = cfg;
            analysis.channels.resize(data_span.size());

            std::vector<std::vector<double>> channel_values;
            channel_values.reserve(data_span.size());

            for (size_t channel = 0; channel < data_span.size(); ++channel) {
                const std::span<const double> samples { data_span[channel].data(), data_span[channel].size() };
                const size_t windows = samples.size() < cfg.window_size
                    ? 0
                    : (samples.size() - cfg.window_size) / cfg.hop_size + 1;

                auto& result = analysis.channels[channel];
                if (cfg.keep_profiles) {
                    result.autocorrelation.first_lag = 0;
                    result.autocorrelation.lag_count = max_lag + 1;
                    result.mean_difference.first_lag = min_lag;
                    result.mean_difference.lag_count = max_lag - min_lag + 1;
                }
                for (size_t first = 0; first < windows; first += k_chunk_windows) {
                    const size_t count = std::min(k_chunk_windows, windows - first);
                    measure_chunk(result, samples, first, count, min_lag, max_lag, cfg);
                }
                derive_changes(result);
                select_values(result, static_cast<WaveformMethod>(cfg.method));
                channel_values.push_back(result.values);
            }

            this->store_current_analysis(analysis);

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Waveform analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    static constexpr size_t k_chunk_windows = 4096;
    static constexpr uint32_t k_hop_divisor = 512;
    static constexpr double k_variance_floor = 1e-12;
    static constexpr double k_strength_ceiling = 1.0 - 1e-12;

    mutable std::shared_ptr<GpuDispatchRecorder> m_recording;
    mutable std::unique_ptr<SegmentedReductionContext> m_energy;
    mutable std::unique_ptr<SegmentedReductionContext> m_sum;
    mutable std::unique_ptr<SegmentedReductionContext> m_peak;

    struct LaggedPass {
        std::unique_ptr<WordPairExecutionContext> context;
        GpuComputeConfig config;
    };

    mutable LaggedPass m_crossings;
    mutable LaggedPass m_correlation;
    mutable LaggedPass m_distance;

    static uint32_t shortest_lag(const WaveformAnalyzerConfig& cfg)
    {
        return std::max<uint32_t>(1, static_cast<uint32_t>(cfg.sample_rate / cfg.high_frequency));
    }

    static uint32_t longest_lag(const WaveformAnalyzerConfig& cfg)
    {
        return static_cast<uint32_t>(cfg.sample_rate / cfg.low_frequency);
    }

    static void validate_config(const WaveformAnalyzerConfig& cfg)
    {
        const auto fail = [](std::string_view message) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}", message);
        };
        if (!std::isfinite(cfg.sample_rate) || cfg.sample_rate <= 0.0) {
            fail("Sample rate must be finite and greater than 0");
        }
        if (!std::isfinite(cfg.low_frequency) || !std::isfinite(cfg.high_frequency)
            || cfg.low_frequency <= 0.0 || cfg.high_frequency <= cfg.low_frequency
            || cfg.high_frequency >= cfg.sample_rate / 2.0) {
            fail("Frequencies must satisfy 0 < low < high < sample rate / 2");
        }
        const uint32_t min_lag = shortest_lag(cfg);
        const uint32_t max_lag = longest_lag(cfg);
        if (min_lag >= max_lag) {
            fail("The frequency range spans fewer than two lags");
        }
        if (max_lag >= std::numeric_limits<uint16_t>::max()) {
            fail("The longest period is too long to search");
        }
        if (cfg.window_size < 2U * max_lag) {
            fail("The window must hold at least twice the longest period");
        }
        if (cfg.hop_size == 0 || k_hop_divisor % cfg.hop_size != 0) {
            fail("The hop size must divide 512");
        }
    }

    void ensure_contexts() const
    {
        if (m_recording) {
            return;
        }
        using Portal::Graphics::KernelOp;
        using Portal::Graphics::PairMap;
        using Portal::Graphics::ReductionMap;

        m_recording = std::make_shared<GpuDispatchRecorder>(true);
        m_energy = std::make_unique<SegmentedReductionContext>(KernelOp::Sum, ReductionMap::Square);
        m_sum = std::make_unique<SegmentedReductionContext>(KernelOp::Sum, ReductionMap::Identity);
        m_peak = std::make_unique<SegmentedReductionContext>(KernelOp::Max, ReductionMap::Abs);
        m_crossings = make_lagged(PairMap::SignChange, false);
        m_correlation = make_lagged(PairMap::Product, true);
        m_distance = make_lagged(PairMap::AbsDifference, false);
        for (GpuDispatchCore* context : std::initializer_list<GpuDispatchCore*> {
                 m_energy.get(), m_sum.get(), m_peak.get(), m_crossings.context.get(),
                 m_correlation.context.get(), m_distance.context.get() }) {
            context->set_recording(m_recording);
        }
    }

    static LaggedPass make_lagged(Portal::Graphics::PairMap map, bool centred)
    {
        const auto spec = lagged_pair_spec(map, centred);
        auto config = config_from_spec(spec);
        auto context = std::make_unique<WordPairExecutionContext>(config, bindings_from_spec(spec), false, "LaggedPair");
        return { std::move(context), std::move(config) };
    }

    static void record_lagged(LaggedPass& pass, std::span<const double> chunk, const SegmentGeometry& geometry,
        uint32_t first_lag, uint32_t lag_count)
    {
        pass.context->record_stages({ lagged_pair_stage(*pass.context, pass.config, chunk, geometry, first_lag, lag_count) });
    }

    static std::vector<double> download_lagged(LaggedPass& pass, size_t windows, uint32_t lag_count)
    {
        return pass.context->download_words(1, windows * lag_count);
    }

    template <typename Context>
    static std::vector<double> collect(Context& context)
    {
        return context.decode_primary(context.collect_result());
    }

    void measure_chunk(ChannelWaveform& out, std::span<const double> samples, size_t first, size_t count,
        uint32_t min_lag, uint32_t max_lag, const WaveformAnalyzerConfig& cfg) const
    {
        const size_t begin = first * cfg.hop_size;
        const size_t end = (first + count - 1) * cfg.hop_size + cfg.window_size;
        const auto chunk = samples.subspan(begin, end - begin);
        const SegmentGeometry geometry { .length = cfg.window_size, .stride = cfg.hop_size, .offset = 0, .count = static_cast<uint32_t>(count) };
        const uint32_t correlation_lags = max_lag + 1;
        const uint32_t distance_lags = max_lag - min_lag + 1;

        m_energy->stage(chunk, geometry);
        m_sum->stage(chunk, geometry);
        m_peak->stage(chunk, geometry);

        const Datum<std::vector<Kakshya::DataVariant>> none {};
        static_cast<void>(m_energy->dispatch_async(none));
        static_cast<void>(m_sum->dispatch_async(none));
        static_cast<void>(m_peak->dispatch_async(none));
        record_lagged(m_crossings, chunk, geometry, 1, 1);
        record_lagged(m_correlation, chunk, geometry, 0, correlation_lags);
        if (cfg.keep_profiles) {
            record_lagged(m_distance, chunk, geometry, min_lag, distance_lags);
        }

        const auto energy = collect(*m_energy);
        const auto sum = collect(*m_sum);
        const auto peak = collect(*m_peak);
        const auto crossings = download_lagged(m_crossings, count, 1);
        const auto correlation = download_lagged(m_correlation, count, correlation_lags);
        const auto distance = cfg.keep_profiles ? download_lagged(m_distance, count, distance_lags) : std::vector<double> {};

        const double window = static_cast<double>(cfg.window_size);
        for (size_t w = 0; w < count; ++w) {
            const size_t start = (first + w) * cfg.hop_size;
            out.window_positions.emplace_back(start, start + cfg.window_size);

            const double level = std::sqrt(energy[w] / window);
            out.level.push_back(level);
            out.crossing_rate.push_back(crossings[w] / (window - 1.0));
            out.peak.push_back(peak[w]);
            out.dc_offset.push_back(sum[w] / window);
            out.crest_factor.push_back(level > 0.0 ? peak[w] / level : 0.0);

            const double* row = correlation.data() + w * correlation_lags;
            const bool varying = row[0] >= k_variance_floor;
            double best = 0.0;
            uint32_t best_lag = 0;
            if (varying) {
                for (uint32_t lag = min_lag; lag <= max_lag; ++lag) {
                    const double strength = row[lag] / row[0];
                    if (strength > best) {
                        best = strength;
                        best_lag = lag;
                    }
                }
            }
            const double bounded = std::min(best, k_strength_ceiling);
            out.periodicity.push_back(best);
            out.period.push_back(static_cast<double>(best_lag));
            out.frequency.push_back(best_lag > 0 ? cfg.sample_rate / static_cast<double>(best_lag) : 0.0);
            out.harmonicity.push_back(best > 0.0 ? bounded / (1.0 - bounded) : 0.0);

            if (cfg.keep_profiles) {
                for (uint32_t lag = 0; lag < correlation_lags; ++lag) {
                    out.autocorrelation.values.push_back(varying ? row[lag] / row[0] : 0.0);
                }
                const double* gaps = distance.data() + w * distance_lags;
                for (uint32_t column = 0; column < distance_lags; ++column) {
                    out.mean_difference.values.push_back(gaps[column] / (window - static_cast<double>(min_lag + column)));
                }
            }
        }
    }

    static void derive_changes(ChannelWaveform& out)
    {
        const size_t windows = out.level.size();
        out.level_change.assign(windows, 0.0);
        out.period_change.assign(windows, 0.0);
        for (size_t w = 1; w < windows; ++w) {
            out.level_change[w] = out.level[w] - out.level[w - 1];
            if (out.period[w] > 0.0 && out.period[w - 1] > 0.0) {
                out.period_change[w] = std::abs(out.period[w] - out.period[w - 1]) / out.period[w];
            }
        }
    }

    static void select_values(ChannelWaveform& out, WaveformMethod method)
    {
        switch (method) {
        case WaveformMethod::CROSSING_RATE:
            out.values = out.crossing_rate;
            break;
        case WaveformMethod::PERIODICITY:
            out.values = out.periodicity;
            break;
        case WaveformMethod::PERIOD:
            out.values = out.period;
            break;
        case WaveformMethod::FREQUENCY:
            out.values = out.frequency;
            break;
        case WaveformMethod::PEAK:
            out.values = out.peak;
            break;
        case WaveformMethod::DC_OFFSET:
            out.values = out.dc_offset;
            break;
        case WaveformMethod::CREST_FACTOR:
            out.values = out.crest_factor;
            break;
        case WaveformMethod::HARMONICITY:
            out.values = out.harmonicity;
            break;
        case WaveformMethod::LEVEL_CHANGE:
            out.values = out.level_change;
            break;
        case WaveformMethod::PERIOD_CHANGE:
            out.values = out.period_change;
            break;
        case WaveformMethod::LEVEL:
        default:
            out.values = out.level;
            break;
        }
    }
};

/// Standard waveform analyzer: DataVariant -> MatrixXd
using StandardWaveformAnalyzer = WaveformAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container waveform analyzer: SignalContainer -> MatrixXd
using ContainerWaveformAnalyzer = WaveformAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
