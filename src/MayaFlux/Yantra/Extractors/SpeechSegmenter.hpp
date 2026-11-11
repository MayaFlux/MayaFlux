#pragma once

#include "Extractor.hpp"

#include "MayaFlux/Kinesis/Discrete/Extract.hpp"
#include "MayaFlux/Kinesis/Discrete/IntervalScan.hpp"
#include "MayaFlux/Yantra/Analyzers/VadAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/WaveformAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/IntervalExtractor.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file SpeechSegmenter.hpp
 * @brief Where speech is in a recording, with optionally how voiced each stretch is and its pitch.
 */

namespace MayaFlux::Yantra {

/**
 * @struct SpeechSettings
 * @brief How speech runs are found and measured.
 *
 * The scan follows Silero's get_speech_timestamps: a run starts at threshold,
 * ends after min_silence without speech, and is widened by padding on both
 * sides. Runs shorter than min_speech are dropped. max_run caps one speech
 * interval in samples, 0 for no cap. All lengths are in samples at 16 kHz.
 *
 * With measure on, each speech interval also gets a voiced fraction and a
 * pitch from the waveform windows whose start lies in it. A window is voiced
 * when its periodicity, divided by the share of pairs its lag can pair, is at
 * least voicing_threshold. The threshold is a starting choice, not a measured
 * one.
 */
struct SpeechSettings {
    /** Speech probability at or above which speech starts. */
    double threshold { 0.5 };
    /** Shortest speech run kept, in samples. */
    uint32_t min_speech { 4000 };
    /** Shortest silence that ends a speech run, in samples. */
    uint32_t min_silence { 1600 };
    /** Samples added on each side of a speech run. */
    uint32_t padding { 480 };
    /** Longest speech interval in samples, 0 for no cap. */
    uint32_t max_run { 0 };
    /** Also measure the voiced fraction and pitch of each speech interval. */
    bool measure { false };
    /** Periodicity at or above which a window is voiced. */
    double voicing_threshold { 0.5 };
};

/**
 * @struct SpeechSegmenterConfig
 * @brief Settings of a SpeechSegmenter.
 *
 * sample_rate states the rate of the input and is refused unless it is 16000,
 * as the network runs at that rate only.
 */
struct SpeechSegmenterConfig : ExtractorConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            ExtractorConfig::describe_facets(),
            std::tuple {
                FacetMember<&SpeechSegmenterConfig::sample_rate> {},
                FacetMember<&SpeechSegmenterConfig::speech> {},
            });
    }

    /** Sample rate in Hz of the input. Only 16000 is accepted. */
    double sample_rate { static_cast<double>(SileroVadModel::sample_rate) };
    /** How speech is found and measured. */
    SpeechSettings speech {};
};

/**
 * @struct SpeechSegmentation
 * @brief Record of one SpeechSegmenter run, stored on the output's facets.
 *
 * speech holds the speech intervals as half open sample ranges. voiced_fraction
 * and f0 are filled only when the run measured, one entry per interval: the
 * share of its windows that are voiced, and the median frequency in Hz of the
 * voiced ones, 0 when none is.
 */
struct MAYAFLUX_API SpeechSegmentation {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&SpeechSegmentation::config> {},
            FacetMember<&SpeechSegmentation::speech> {},
            FacetMember<&SpeechSegmentation::voiced_fraction> {},
            FacetMember<&SpeechSegmentation::f0> {},
        };
    }

    /** Settings the segmentation was produced under. */
    SpeechSegmenterConfig config;
    /** Speech intervals, half open sample ranges. */
    std::vector<std::pair<size_t, size_t>> speech;
    /** Voiced share of each interval's windows, empty unless measured. */
    std::vector<double> voiced_fraction;
    /** Median frequency of each interval's voiced windows, empty unless measured. */
    std::vector<double> f0;
};

/**
 * @brief The interval scan settings of speech runs, for a recording of the given length.
 *
 * Scores are the probabilities of 512 sample chunks, and the leave threshold is
 * the one IntervalExtractorConfig gives by default, as in Silero.
 */
[[nodiscard]] inline IntervalExtractorConfig speech_scan_settings(const SpeechSettings& speech, size_t samples)
{
    IntervalExtractorConfig scan;
    scan.scan.enter_threshold = speech.threshold;
    scan.scan.samples_per_score = SileroVadModel::chunk_samples;
    scan.scan.min_run = speech.min_speech;
    scan.scan.min_gap = speech.min_silence;
    scan.scan.padding = speech.padding;
    scan.scan.max_run = speech.max_run;
    scan.flush = true;
    scan.length = static_cast<uint32_t>(samples);
    return scan;
}

/**
 * @struct SpeechVoicing
 * @brief Voicing of each interval, as SpeechSegmentation holds it.
 */
struct SpeechVoicing {
    /** Voiced share of each interval's windows. */
    std::vector<double> voiced_fraction;
    /** Median frequency of each interval's voiced windows. */
    std::vector<double> f0;
};

/**
 * @brief The waveform analyzer settings the voicing measure uses at the given rate.
 *
 * Unbiased periodicity, so that one voicing threshold serves every pitch.
 */
[[nodiscard]] inline WaveformAnalyzerConfig voicing_analyzer_config(double sample_rate)
{
    WaveformAnalyzerConfig settings;
    settings.sample_rate = sample_rate;
    settings.unbiased_periodicity = true;
    return settings;
}

/**
 * @brief Voiced share and median frequency of the waveform windows that start in each interval.
 * @param analyzer Analyzer built from voicing_analyzer_config.
 * @param samples One channel of samples.
 * @param speech Half open sample intervals.
 * @param voicing_threshold Periodicity at or above which a window is voiced.
 * @return One entry per interval; 0 for an interval holding no windows or no voiced window.
 */
[[nodiscard]] inline SpeechVoicing measure_voicing(StandardWaveformAnalyzer& analyzer, std::span<const double> samples,
    const std::vector<std::pair<size_t, size_t>>& speech, double voicing_threshold)
{
    SpeechVoicing voicing;
    voicing.voiced_fraction.assign(speech.size(), 0.0);
    voicing.f0.assign(speech.size(), 0.0);
    if (speech.empty() || samples.size() < WaveformAnalyzerConfig {}.window_size) {
        return voicing;
    }

    const auto analysis = analyzer.analyze_waveform(
        std::vector<Kakshya::DataVariant> { std::vector<double>(samples.begin(), samples.end()) });
    if (analysis.channels.empty()) {
        return voicing;
    }
    const auto& waveform = analysis.channels[0];

    std::vector<size_t> starts;
    starts.reserve(waveform.window_positions.size());
    for (const auto& position : waveform.window_positions) {
        starts.push_back(position.first);
    }
    voicing.voiced_fraction = Kinesis::Discrete::interval_share_above(
        waveform.periodicity, starts, speech, voicing_threshold);

    std::vector<double> voiced_frequency;
    std::vector<size_t> voiced_starts;
    for (size_t window = 0; window < starts.size() && window < waveform.periodicity.size(); ++window) {
        if (waveform.periodicity[window] >= voicing_threshold) {
            voiced_frequency.push_back(waveform.frequency[window]);
            voiced_starts.push_back(starts[window]);
        }
    }
    voicing.f0 = Kinesis::Discrete::interval_median(voiced_frequency, voiced_starts, speech);
    return voicing;
}

/**
 * @class SpeechSegmenter
 * @brief The speech intervals of a whole mono 16 kHz recording.
 *
 * Runs Silero VAD over the recording and thresholds its probabilities into
 * speech intervals. With SpeechSettings::measure it also runs a
 * WaveformAnalyzer once over the recording and reports, per interval, the
 * share of voiced windows and their median frequency. Without it no waveform
 * analysis is made.
 *
 * Voiced means periodic in the 70 to 400 Hz band, which a hum or a sung note
 * also is; the speech intervals are what say the sound is speech.
 *
 * OutputType selects the output: Kakshya::RegionGroup (one region per speech
 * interval, the default) or std::vector<Kakshya::DataVariant> (the speech
 * audio, concatenated). The facet SpeechSegmentation is always set. Blocking,
 * one channel, not in the default registry. It must not run on the audio thread.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Kakshya::RegionGroup>
class SpeechSegmenter : public Extractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Extractor<InputType, OutputType>;
    using supports = Supports<SpeechSegmenterConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from the weights of the speech network.
     * @param vad_weights Tensors of Silero VAD at 16 kHz, as in silero_vad_16k.safetensors.
     * @param config Segmentation settings.
     * @throws std::invalid_argument When diagnose refuses the config, or the weights are refused by the network.
     */
    explicit SpeechSegmenter(const Kakshya::TensorSet& vad_weights, SpeechSegmenterConfig config = {})
        : m_vad(vad_weights, vad_settings(config))
    {
        if (const std::string reason = diagnose(config); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "SpeechSegmenter: {}", reason);
        }
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
     * @brief Why a config cannot be segmented with.
     * @return The reason, empty when usable.
     */
    [[nodiscard]] static std::string diagnose(const SpeechSegmenterConfig& config)
    {
        if (config.sample_rate != static_cast<double>(SileroVadModel::sample_rate)) {
            return std::format("sample_rate {} is not the {} Hz the network runs at; convert the input first, for example with Kinesis::Discrete::resample",
                config.sample_rate, SileroVadModel::sample_rate);
        }
        if (!std::isfinite(config.speech.voicing_threshold) || config.speech.voicing_threshold <= 0.0) {
            return "voicing_threshold must be finite and greater than 0";
        }
        const auto scan = speech_scan_settings(config.speech, 0);
        return Kinesis::Discrete::diagnose_interval_scan(interval_scan_config(scan.scan, scan.length));
    }

    /**
     * @brief Segment samples under the given settings, without the Datum wrapper.
     * @param samples One channel of 16 kHz samples.
     * @param config Settings for this call; the stored config is not used or changed.
     * @return The record of the run.
     * @throws std::invalid_argument When diagnose refuses the config.
     */
    SpeechSegmentation segment(std::span<const double> samples, const SpeechSegmenterConfig& config)
    {
        if (const std::string reason = diagnose(config); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "SpeechSegmenter: {}", reason);
        }

        SpeechSegmentation record;
        record.config = config;
        if (samples.empty()) {
            return record;
        }
        find_speech(samples, config, record);
        if (config.speech.measure) {
            measure_speech(samples, config, record);
        }
        return record;
    }

    /**
     * @brief Type-safe segmentation.
     * @return The record of the run.
     */
    SpeechSegmentation extract_segments(const input_type& data)
    {
        const auto result = this->apply_operation(data);
        const auto* record = result.facets.template get<SpeechSegmentation>();
        if (record == nullptr) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "Speech segmentation produced no result");
        }
        return *record;
    }

    SpeechSegmentation extract_segments(const InputType& data)
    {
        return this->extract_segments(input_type { data });
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
        return "SpeechSegmenter";
    }

    output_type extract_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<SpeechSegmenterConfig>();

            auto [channels, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(input));
            if (channels.size() != 1) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "SpeechSegmenter analyses one channel and was given {}", channels.size());
            }
            const std::span<const double> samples { channels[0].data(), channels[0].size() };

            const SpeechSegmentation record = segment(samples, cfg);
            output_type output = build_output(record, samples, info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(record);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Speech segmentation failed: {}", e.what());
            output_type error_result {};
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    static VadAnalyzerConfig vad_settings(const SpeechSegmenterConfig& config)
    {
        VadAnalyzerConfig settings;
        settings.flush_tail = true;
        settings.sample_rate = config.sample_rate;
        return settings;
    }

    void find_speech(std::span<const double> samples, const SpeechSegmenterConfig& cfg, SpeechSegmentation& record)
    {
        m_vad.reset();
        const auto probabilities = m_vad.analyze_speech(
            std::vector<Kakshya::DataVariant> { std::vector<double>(samples.begin(), samples.end()) })
                                       .probabilities;
        if (probabilities.empty()) {
            return;
        }

        Eigen::MatrixXd scores(static_cast<Eigen::Index>(probabilities.size()), 1);
        for (size_t chunk = 0; chunk < probabilities.size(); ++chunk) {
            scores(static_cast<Eigen::Index>(chunk), 0) = probabilities[chunk];
        }
        m_intervals.set_config(speech_scan_settings(cfg.speech, samples.size()));
        const auto intervals = m_intervals.extract_intervals(scores);
        if (!intervals.channels.empty()) {
            record.speech = intervals.channels[0].intervals;
        }
    }

    void measure_speech(std::span<const double> samples, const SpeechSegmenterConfig& cfg, SpeechSegmentation& record)
    {
        if (!m_waveform) {
            m_waveform.emplace(voicing_analyzer_config(cfg.sample_rate));
        }
        auto voicing = measure_voicing(*m_waveform, samples, record.speech, cfg.speech.voicing_threshold);
        record.voiced_fraction = std::move(voicing.voiced_fraction);
        record.f0 = std::move(voicing.f0);
    }

    output_type build_output(const SpeechSegmentation& record, std::span<const double> samples, DataStructureInfo& info)
    {
        if constexpr (std::is_same_v<OutputType, Kakshya::RegionGroup>) {
            ChannelIntervals intervals;
            intervals.intervals = record.speech;
            output_type output;
            output.data = interval_regions(intervals, 0, 0, "speech");
            return output;
        } else {
            std::vector<std::vector<double>> channel { Kinesis::Discrete::slice_intervals(samples, record.speech) };
            return this->convert_result(channel, info);
        }
    }

    StandardVadAnalyzer m_vad;
    MatrixIntervalExtractor m_intervals;
    std::optional<StandardWaveformAnalyzer> m_waveform;
};

/// Regions: one region per speech interval
using RegionSpeechSegmenter = SpeechSegmenter<std::vector<Kakshya::DataVariant>, Kakshya::RegionGroup>;

/// Audio: the speech intervals, concatenated
using AudioSpeechSegmenter = SpeechSegmenter<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
