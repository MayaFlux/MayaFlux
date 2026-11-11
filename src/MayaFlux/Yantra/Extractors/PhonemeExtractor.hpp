#pragma once

#include "Extractor.hpp"

#include "MayaFlux/Kinesis/Discrete/Extract.hpp"
#include "MayaFlux/Kinesis/Discrete/IntervalScan.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/Analyzers/PhonemeAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/VadAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/IntervalExtractor.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file PhonemeExtractor.hpp
 * @brief The phonemes of a whole recording: speech is found first, then the phoneme network runs on each speech interval.
 */

namespace MayaFlux::Yantra {

/**
 * @enum PhonemeSelection
 * @brief Which recognised phonemes the output holds.
 */
enum class PhonemeSelection : uint8_t {
    ALL, ///< Every recognised phoneme
    VOWELS, ///< Vowel phonemes only
    CONSONANTS ///< Phonemes that are not vowels
};

/**
 * @struct PhonemeExtractorConfig
 * @brief Settings of a PhonemeExtractor.
 *
 * sample_rate states the rate of the input and is refused unless it is 16000,
 * as the networks run at that rate only. The speech_* fields are passed to the
 * speech scan with the rule of Silero's get_speech_timestamps: a speech run
 * starts at speech_threshold, ends after min_silence without speech, and is
 * widened by speech_padding on both sides. Runs shorter than min_speech are
 * dropped. merge_adjacent joins output ranges that touch, for slicing.
 */
struct PhonemeExtractorConfig : ExtractorConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            ExtractorConfig::describe_facets(),
            std::tuple {
                FacetMember<&PhonemeExtractorConfig::method> {},
                FacetMember<&PhonemeExtractorConfig::sample_rate> {},
                FacetMember<&PhonemeExtractorConfig::merge_adjacent> {},
                FacetMember<&PhonemeExtractorConfig::speech_threshold> {},
                FacetMember<&PhonemeExtractorConfig::min_speech> {},
                FacetMember<&PhonemeExtractorConfig::min_silence> {},
                FacetMember<&PhonemeExtractorConfig::speech_padding> {},
            });
    }

    /** Which phonemes the output holds, as a PhonemeSelection value. */
    Reflect::EnumByte<PhonemeSelection> method { PhonemeSelection::ALL };
    /** Sample rate in Hz of the input. Only 16000 is accepted. */
    double sample_rate { static_cast<double>(SileroVadModel::sample_rate) };
    /** Join output ranges that touch, so a run of phonemes is one range. */
    bool merge_adjacent { false };
    /** Speech probability at or above which speech starts. */
    double speech_threshold { 0.5 };
    /** Shortest speech run kept, in samples. */
    uint32_t min_speech { 4000 };
    /** Shortest silence that ends a speech run, in samples. */
    uint32_t min_silence { 1600 };
    /** Samples added on each side of a speech run. */
    uint32_t speech_padding { 480 };
};

/**
 * @struct PhonemeExtraction
 * @brief Record of one PhonemeExtractor run, stored on the output's facets.
 *
 * speech holds the speech intervals, as half open sample ranges of the whole
 * recording. events holds every recognised phoneme, whatever the selection, in
 * recording order, and segments gives for each the index of its speech
 * interval. The sample, start and end of an event count from the start of the
 * recording; its frame counts from the start of its speech interval.
 */
struct MAYAFLUX_API PhonemeExtraction {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&PhonemeExtraction::config> {},
            FacetMember<&PhonemeExtraction::speech> {},
            FacetMember<&PhonemeExtraction::events> {},
            FacetMember<&PhonemeExtraction::segments> {},
        };
    }

    /** Settings the extraction was produced under. */
    PhonemeExtractorConfig config;
    /** Speech intervals, half open sample ranges of the recording. */
    std::vector<std::pair<size_t, size_t>> speech;
    /** Every recognised phoneme, in recording order. */
    std::vector<PhonemeEvent> events;
    /** For each event, the index of the speech interval it lies in. */
    std::vector<uint32_t> segments;
};

/**
 * @brief Field layout of the records a PhonemeExtractor gives: the phoneme record layout with the speech interval index added.
 *
 * segment is the index of the speech interval the phoneme lies in, the same as
 * PhonemeExtraction::segments.
 */
[[nodiscard]] inline Kakshya::CompositeLayout phoneme_extraction_layout()
{
    auto layout = phoneme_record_layout();
    layout.add_field<uint32_t>("segment");
    return layout;
}

/**
 * @brief The speech scan settings a PhonemeExtractor uses, for a recording of the given length.
 *
 * Scores are the probabilities of 512 sample chunks. The leave threshold is
 * the one IntervalExtractorConfig gives by default, as in Silero. Runs are
 * capped at HubertPhonemeModel::max_samples so that each speech interval fits
 * one call of the phoneme network.
 */
[[nodiscard]] inline IntervalExtractorConfig speech_scan_settings(const PhonemeExtractorConfig& config, size_t samples)
{
    IntervalExtractorConfig speech;
    speech.enter_threshold = config.speech_threshold;
    speech.samples_per_score = SileroVadModel::chunk_samples;
    speech.min_run = config.min_speech;
    speech.min_gap = config.min_silence;
    speech.padding = config.speech_padding;
    speech.max_run = HubertPhonemeModel::max_samples;
    speech.flush = true;
    speech.length = static_cast<uint32_t>(samples);
    return speech;
}

/**
 * @class PhonemeExtractor
 * @brief The phonemes of a whole mono 16 kHz recording, with the pauses between speech taken out.
 *
 * Finds speech with VadAnalyzer, then runs the phoneme network once on each
 * speech interval, so no call exceeds its 30 s limit and nothing runs on
 * silence. Phoneme ranges are estimates (PhonemeEvent). Pauses shorter than
 * min_silence stay inside a range.
 *
 * OutputType selects the output: Kakshya::CompositeArray (records, the
 * default), Kakshya::RegionGroup (regions) or std::vector<Kakshya::DataVariant>
 * (audio of the selected phonemes). Blocking, one channel, not in the default
 * registry. It must not run on the audio thread.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Kakshya::CompositeArray>
class PhonemeExtractor : public Extractor<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Extractor<InputType, OutputType>;
    using supports = Supports<PhonemeExtractorConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from the weights of both networks.
     * @param vad_weights Tensors of Silero VAD at 16 kHz, as in silero_vad_16k.safetensors.
     * @param phoneme_weights Tensors of the HuBERT phoneme network, as in hubert_base_phoneme_en.safetensors.
     * @param config Extraction settings.
     * @throws std::invalid_argument When diagnose refuses the config, or either weight set is refused by its network.
     */
    PhonemeExtractor(const Kakshya::TensorSet& vad_weights, const Kakshya::TensorSet& phoneme_weights, PhonemeExtractorConfig config = {})
        : m_vad(vad_weights, vad_settings(config))
        , m_phonemes(phoneme_weights, phoneme_settings(config))
    {
        if (const std::string reason = diagnose(config); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "PhonemeExtractor: {}", reason);
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
     * @brief Why a config cannot be extracted with.
     * @return The reason, empty when usable.
     */
    [[nodiscard]] static std::string diagnose(const PhonemeExtractorConfig& config)
    {
        if (config.sample_rate != static_cast<double>(SileroVadModel::sample_rate)) {
            return std::format("sample_rate {} is not the {} Hz the networks run at; convert the input first, for example with Kinesis::Discrete::resample",
                config.sample_rate, SileroVadModel::sample_rate);
        }
        return Kinesis::Discrete::diagnose_interval_scan(
            speech_scan(speech_scan_settings(config, 0)));
    }

    /**
     * @brief Type-safe phoneme extraction.
     * @return The record of the run.
     */
    PhonemeExtraction extract_phonemes(const input_type& data)
    {
        const auto result = this->apply_operation(data);
        const auto* record = result.facets.template get<PhonemeExtraction>();
        if (record == nullptr) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "Phoneme extraction produced no result");
        }
        return *record;
    }

    PhonemeExtraction extract_phonemes(const InputType& data)
    {
        return this->extract_phonemes(input_type { data });
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
        return "PhonemeExtractor";
    }

    output_type extract_implementation(const input_type& input) override
    {
        try {
            const auto& cfg = this->template config<PhonemeExtractorConfig>();
            if (const std::string reason = diagnose(cfg); !reason.empty()) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "PhonemeExtractor: {}", reason);
            }

            auto [channels, info] = OperationHelper::extract_structured_double(const_cast<input_type&>(input));
            if (channels.size() != 1) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "PhonemeExtractor analyses one channel and was given {}", channels.size());
            }
            const std::span<const double> samples { channels[0].data(), channels[0].size() };

            PhonemeExtraction record;
            record.config = cfg;
            if (!samples.empty()) {
                find_speech(samples, cfg, record);
                recognise_phonemes(samples, record);
            }

            const auto rows = selected_rows(record);
            output_type output = build_output(record, rows, samples, info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(record);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Phoneme extraction failed: {}", e.what());
            output_type error_result = empty_output();
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    static VadAnalyzerConfig vad_settings(const PhonemeExtractorConfig& config)
    {
        VadAnalyzerConfig settings;
        settings.flush_tail = true;
        settings.sample_rate = config.sample_rate;
        return settings;
    }

    static PhonemeAnalyzerConfig phoneme_settings(const PhonemeExtractorConfig& config)
    {
        PhonemeAnalyzerConfig settings;
        settings.method = PhonemeMethod::HELD_CLASSES;
        settings.sample_rate = config.sample_rate;
        return settings;
    }

    static Kinesis::Discrete::IntervalScanConfig speech_scan(const IntervalExtractorConfig& speech)
    {
        return {
            .enter = speech.enter_threshold,
            .leave = std::max(speech.enter_threshold - speech.leave_margin, speech.leave_floor),
            .gate_threshold = speech.gate_threshold,
            .hop = speech.samples_per_score,
            .first_sample = speech.first_sample,
            .min_run = speech.min_run,
            .min_gap = speech.min_gap,
            .padding = speech.padding,
            .gap_at_limit = speech.gap_at_limit,
            .max_run = speech.max_run,
            .use_longest_gap = speech.use_longest_gap,
            .length = speech.length,
            .emit_on_close = speech.emit_on_close,
        };
    }

    void find_speech(std::span<const double> samples, const PhonemeExtractorConfig& cfg, PhonemeExtraction& record)
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
        m_intervals.set_config(speech_scan_settings(cfg, samples.size()));
        const auto intervals = m_intervals.extract_intervals(scores);
        if (!intervals.channels.empty()) {
            record.speech = intervals.channels[0].intervals;
        }
    }

    void recognise_phonemes(std::span<const double> samples, PhonemeExtraction& record)
    {
        for (uint32_t index = 0; index < record.speech.size(); ++index) {
            const auto [first, last] = record.speech[index];
            if (last <= first || last > samples.size() || last - first < HubertPhonemeModel::frame_span) {
                continue;
            }
            const auto part = samples.subspan(first, last - first);
            const auto analysis = m_phonemes.analyze_phonemes(
                std::vector<Kakshya::DataVariant> { std::vector<double>(part.begin(), part.end()) });
            for (auto event : analysis.events) {
                event.sample += first;
                event.start += first;
                event.end += first;
                record.events.push_back(event);
                record.segments.push_back(index);
            }
        }
    }

    static bool is_selected(const PhonemeEvent& event, PhonemeSelection selection)
    {
        switch (selection) {
        case PhonemeSelection::VOWELS:
            return HubertPhonemeModel::is_vowel(event.phoneme);
        case PhonemeSelection::CONSONANTS:
            return !HubertPhonemeModel::is_vowel(event.phoneme);
        case PhonemeSelection::ALL:
        default:
            return true;
        }
    }

    static std::vector<size_t> selected_rows(const PhonemeExtraction& record)
    {
        const auto selection = static_cast<PhonemeSelection>(record.config.method);
        std::vector<size_t> rows;
        for (size_t row = 0; row < record.events.size(); ++row) {
            if (is_selected(record.events[row], selection)) {
                rows.push_back(row);
            }
        }
        return rows;
    }

    static output_type empty_output()
    {
        if constexpr (CompositeArrayData<OutputType>) {
            return output_type { Kakshya::CompositeArray(phoneme_extraction_layout()) };
        } else {
            return output_type {};
        }
    }

    output_type build_output(const PhonemeExtraction& record, const std::vector<size_t>& rows,
        std::span<const double> samples, DataStructureInfo& info)
    {
        if constexpr (CompositeArrayData<OutputType>) {
            info.composite_layout = phoneme_extraction_layout();
            std::vector<std::vector<double>> columns(8);
            for (const size_t row : rows) {
                const auto& event = record.events[row];
                columns[0].push_back(static_cast<double>(event.phoneme));
                columns[1].push_back(static_cast<double>(event.frame));
                columns[2].push_back(static_cast<double>(event.sample));
                columns[3].push_back(static_cast<double>(event.start));
                columns[4].push_back(static_cast<double>(event.end));
                columns[5].push_back(static_cast<double>(event.probability));
                columns[6].push_back(HubertPhonemeModel::is_vowel(event.phoneme) ? 1.0 : 0.0);
                columns[7].push_back(static_cast<double>(record.segments[row]));
            }
            output_type output = this->convert_result(columns, info);
            for (size_t k = 0; k < rows.size(); ++k) {
                output.data.set_text(k, "phoneme", HubertPhonemeModel::label(record.events[rows[k]].phoneme));
            }
            return output;
        } else if constexpr (std::is_same_v<OutputType, Kakshya::RegionGroup>) {
            Kakshya::RegionGroup group("phonemes");
            for (const size_t row : rows) {
                const auto& event = record.events[row];
                if (event.end <= event.start) {
                    continue;
                }
                auto region = Kakshya::Region::audio_span(event.start, event.end - 1, 0, 0,
                    std::string(HubertPhonemeModel::label(event.phoneme)));
                region.set(phoneme_probability, static_cast<double>(event.probability));
                region.set(phoneme_class, event.phoneme);
                group.add_region(region);
            }
            output_type output;
            output.data = std::move(group);
            return output;
        } else {
            std::vector<std::pair<size_t, size_t>> ranges;
            for (const size_t row : rows) {
                const auto& event = record.events[row];
                if (event.end > event.start) {
                    ranges.emplace_back(event.start, event.end);
                }
            }
            if (record.config.merge_adjacent) {
                ranges = Kinesis::Discrete::merge_intervals(ranges);
            }
            std::vector<std::vector<double>> channel { Kinesis::Discrete::slice_intervals(samples, ranges) };
            return this->convert_result(channel, info);
        }
    }

    StandardVadAnalyzer m_vad;
    StandardPhonemeAnalyzer m_phonemes;
    MatrixIntervalExtractor m_intervals;
};

/// Records: one Composite record per selected phoneme
using StandardPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, Kakshya::CompositeArray>;

/// Regions: one region per selected phoneme
using RegionPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, Kakshya::RegionGroup>;

/// Audio: the audio of the selected phonemes, concatenated
using AudioPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
