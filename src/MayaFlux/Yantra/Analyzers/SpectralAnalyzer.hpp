#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Kinesis/Discrete/Taper.hpp"
#include "MayaFlux/Yantra/Executors/Audio/FftContext.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file SpectralAnalyzer.hpp
 * @brief Windowed spectral descriptors computed on the GPU, exactly, with no fp64 on the device.
 *
 * Each window is tapered, transformed and reduced against rows of weights on the
 * device, so only a handful of doubles per window come back. That makes the
 * analysis cheap to run over many windows, such as every grain of a long corpus.
 */

namespace MayaFlux::Yantra {

/**
 * @enum SpectralMethod
 * @brief The per-window value a SpectralAnalyzer reports as its output data.
 */
enum class SpectralMethod : uint8_t {
    TOTAL_POWER, ///< Sum of the power spectrum over the bins 0 to window_size / 2
    CENTROID, ///< Power weighted mean bin index, zero for a silent window
    FLUX ///< Sum over bands of the rise in band energy since the previous window, zero for the first
};

/**
 * @struct SpectralAnalyzerConfig
 * @brief Settings of a SpectralAnalyzer.
 *
 * window_size is the transform length: a power of two from 2 to 2048.
 */
struct SpectralAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&SpectralAnalyzerConfig::method> {},
                FacetMember<&SpectralAnalyzerConfig::band_edges> {},
                FacetMember<&SpectralAnalyzerConfig::taper> {},
            });
    }

    /** Value reported as the output data, as a SpectralMethod value. */
    Reflect::EnumByte<SpectralMethod> method { SpectralMethod::TOTAL_POWER };
    /** Ascending band boundaries as fractions of the Nyquist frequency, 0 to 1. N edges make N - 1 bands. */
    std::vector<double> band_edges { 0.0, 0.25, 0.5, 1.0 };
    /** Multiply each window by a Hann taper before the transform. */
    bool taper { true };
};

/**
 * @struct ChannelSpectrum
 * @brief Spectral descriptors of one channel, one entry per window.
 */
struct MAYAFLUX_API ChannelSpectrum {
    /** The value selected by the method. */
    std::vector<double> values;
    /** Power in each band, per window. */
    std::vector<std::vector<double>> band_energies;
    /** First and one past the last sample of each window. */
    std::vector<std::pair<size_t, size_t>> window_positions;
};

/**
 * @struct SpectralAnalysis
 * @brief Analysis result of a SpectralAnalyzer.
 */
struct MAYAFLUX_API SpectralAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&SpectralAnalysis::config> {},
            FacetMember<&SpectralAnalysis::channels> {},
        };
    }

    /** Settings the analysis was produced under. */
    SpectralAnalyzerConfig config;
    std::vector<ChannelSpectrum> channels;
};

/**
 * @class SpectralAnalyzer
 * @brief Windowed spectral descriptors of every channel, run on the GPU.
 *
 * One dispatch per channel transforms all windows and projects each power
 * spectrum onto rows of weights: its total, its first moment and one row per
 * band. The method picks which value becomes the output data; the band energies
 * are always in the analysis. Sums run in emulated double arithmetic on word
 * pairs, so results agree with Kinesis spectral_projection to rounding and the
 * device needs no float64.
 *
 * It is a distinct operation on the vulkan target, not a replacement for an
 * existing analyzer, and has no CPU twin. A channel shorter than one window
 * gives no windows.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class SpectralAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<SpectralAnalyzerConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct a spectral analyzer from its settings.
     * @param config Analysis settings, validated before they are stored.
     */
    explicit SpectralAnalyzer(SpectralAnalyzerConfig config = {})
    {
        validate_config(config);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @struct SpectralBuffer
     * @brief Where the descriptors of the latest record are on the device.
     *
     * Word pair doubles, window major: values_per_window of them per window, the
     * total power, the first moment, then the power of each band.
     */
    struct SpectralBuffer {
        const GpuDispatchCore* context { nullptr };
        uint32_t set { 0 };
        size_t binding { 0 };
        size_t windows { 0 };
        size_t values_per_window { 0 };
    };

    /**
     * @brief Append to a recording the caller shares with other work.
     *
     * With a shared recording the transform goes to the device in the same
     * submission as the work recorded around it. Call it before the first
     * analysis or record. The recording must be compute isolated.
     *
     * @throws std::logic_error when the analyzer has already started.
     */
    void share_recording(std::shared_ptr<GpuDispatchRecorder> recording)
    {
        if (m_context) {
            error<std::logic_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "The recording can only be shared before the analyzer starts");
        }
        m_recording = std::move(recording);
    }

    /**
     * @brief Transform the windows of one channel without waiting for the result.
     *
     * The descriptors stay on the device, in descriptor_buffer, recorded into
     * the analyzer's recording with no submission and no read back. A consumer
     * on the same recording, for instance a WordPairMap ToFloat32 stage that
     * hands them to a network, reads them in place. Bind the consumer after the
     * analyzer's settings are final, since a change of window or bands makes a
     * new context.
     *
     * @param channel Samples of one channel.
     * @return The number of windows recorded.
     */
    [[nodiscard]] size_t record(std::span<const double> channel)
    {
        const auto& cfg = this->template config<SpectralAnalyzerConfig>();
        auto& context = this->context(cfg);
        const size_t windows = channel.size() < cfg.window_size
            ? 0
            : (channel.size() - cfg.window_size) / cfg.hop_size + 1;
        m_resident_windows = windows;
        if (windows == 0) {
            return 0;
        }
        const std::vector<double> taper = cfg.taper ? Kinesis::Discrete::hann(cfg.window_size) : std::vector<double> {};
        context.ensure_shared_buffer(0, FftContext::result_binding, windows * context.descriptors(), GpuBufferBinding::ElementType::VEC2_U32);
        context.record_real(channel,
            { .length = cfg.window_size, .stride = cfg.hop_size, .offset = 0, .count = static_cast<uint32_t>(windows) }, taper);
        return windows;
    }

    /**
     * @brief The device buffer of the descriptors of the latest record. It exists from this call on.
     */
    [[nodiscard]] SpectralBuffer descriptor_buffer()
    {
        const auto& cfg = this->template config<SpectralAnalyzerConfig>();
        auto& context = this->context(cfg);
        context.ensure_shared_buffer(0, FftContext::result_binding, 1, GpuBufferBinding::ElementType::VEC2_U32);
        return { .context = &context, .set = 0, .binding = FftContext::result_binding,
            .windows = m_resident_windows, .values_per_window = context.descriptors() };
    }

    /**
     * @brief Type-safe spectral analysis
     */
    SpectralAnalysis analyze_spectrum(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<SpectralAnalysis>(result);
    }

    SpectralAnalysis analyze_spectrum(const InputType& data)
    {
        return this->analyze_spectrum(input_type { data });
    }

    [[nodiscard]] SpectralAnalysis get_spectral_analysis() const
    {
        return safe_any_cast_or_throw<SpectralAnalysis>(this->get_current_analysis());
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::SPECTRAL;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "SpectralAnalyzer";
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
            const auto& cfg = this->template config<SpectralAnalyzerConfig>();

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            auto& context = this->context(cfg);
            const std::vector<double> taper = cfg.taper ? Kinesis::Discrete::hann(cfg.window_size) : std::vector<double> {};
            const size_t rows = context.descriptors();
            const size_t bands = rows - 2;

            SpectralAnalysis analysis;
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
                if (windows > 0) {
                    const auto projected = context.transform(samples,
                        { .length = cfg.window_size, .stride = cfg.hop_size, .offset = 0, .count = static_cast<uint32_t>(windows) },
                        taper);
                    fill_channel(result, projected, windows, rows, bands, cfg);
                }
                channel_values.push_back(result.values);
            }

            this->store_current_analysis(analysis);

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Spectral analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    struct ContextKey {
        uint32_t window_size {};
        std::vector<double> band_edges;
        bool operator==(const ContextKey&) const = default;
    };

    mutable std::unique_ptr<FftContext> m_context;
    mutable ContextKey m_key;
    std::shared_ptr<GpuDispatchRecorder> m_recording;
    size_t m_resident_windows { 0 };

    static void validate_config(const SpectralAnalyzerConfig& cfg)
    {
        const auto fail = [](std::string_view message) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "{}", message);
        };
        if (cfg.window_size < 2 || cfg.window_size > 2048 || (cfg.window_size & (cfg.window_size - 1)) != 0) {
            fail("Window size must be a power of two from 2 to 2048");
        }
        if (cfg.hop_size == 0) {
            fail("Hop size must be greater than 0");
        }
        if (cfg.band_edges.size() < 2) {
            fail("At least two band edges are needed");
        }
        for (size_t i = 0; i < cfg.band_edges.size(); ++i) {
            const double edge = cfg.band_edges[i];
            if (std::isnan(edge) || edge < 0.0 || edge > 1.0 || (i > 0 && edge <= cfg.band_edges[i - 1])) {
                fail("Band edges must ascend strictly within 0 to 1");
            }
        }
    }

    /**
     * @brief Rows of weights: total, first moment, then one row per band.
     */
    static std::vector<double> weights_for(const SpectralAnalyzerConfig& cfg)
    {
        const size_t bins = static_cast<size_t>(cfg.window_size) / 2 + 1;
        const size_t bands = cfg.band_edges.size() - 1;
        std::vector<double> weights((2 + bands) * bins, 0.0);
        for (size_t k = 0; k < bins; ++k) {
            const double fraction = static_cast<double>(k) / static_cast<double>(bins - 1);
            weights[k] = 1.0;
            weights[bins + k] = static_cast<double>(k);
            for (size_t b = 0; b < bands; ++b) {
                const bool last = b + 1 == bands;
                if (fraction >= cfg.band_edges[b] && (fraction < cfg.band_edges[b + 1] || (last && fraction <= cfg.band_edges[b + 1]))) {
                    weights[(2 + b) * bins + k] = 1.0;
                }
            }
        }
        return weights;
    }

    FftContext& context(const SpectralAnalyzerConfig& cfg) const
    {
        const ContextKey key { cfg.window_size, cfg.band_edges };
        if (!m_context || !(m_key == key)) {
            const auto weights = weights_for(cfg);
            m_context = std::make_unique<FftContext>(cfg.window_size, weights);
            if (m_recording) {
                m_context->set_recording(m_recording);
            }
            m_key = key;
        }
        return *m_context;
    }

    static void fill_channel(ChannelSpectrum& out, const std::vector<double>& projected,
        size_t windows, size_t rows, size_t bands, const SpectralAnalyzerConfig& cfg)
    {
        out.values.resize(windows);
        out.band_energies.resize(windows);
        out.window_positions.reserve(windows);

        for (size_t w = 0; w < windows; ++w) {
            const double* row = projected.data() + w * rows;
            out.band_energies[w].assign(row + 2, row + 2 + bands);
            const size_t start = w * cfg.hop_size;
            out.window_positions.emplace_back(start, start + cfg.window_size);

            switch (static_cast<SpectralMethod>(cfg.method)) {
            case SpectralMethod::CENTROID:
                out.values[w] = row[0] > 0.0 ? row[1] / row[0] : 0.0;
                break;
            case SpectralMethod::FLUX: {
                double rise = 0.0;
                if (w > 0) {
                    for (size_t b = 0; b < bands; ++b) {
                        rise += std::max(0.0, out.band_energies[w][b] - out.band_energies[w - 1][b]);
                    }
                }
                out.values[w] = rise;
                break;
            }
            case SpectralMethod::TOTAL_POWER:
            default:
                out.values[w] = row[0];
                break;
            }
        }
    }
};

/// Standard spectral analyzer: DataVariant -> MatrixXd
using StandardSpectralAnalyzer = SpectralAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container spectral analyzer: SignalContainer -> MatrixXd
using ContainerSpectralAnalyzer = SpectralAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
