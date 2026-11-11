#pragma once

#include "Extractor.hpp"

#include "MayaFlux/Kinesis/Discrete/Extract.hpp"
#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/Analyzers/PhonemeAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/SpeechSegmenter.hpp"
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
 * @struct VoicingGate
 * @brief Trims vowel ranges to the stretches where the waveform is periodic.
 *
 * Vowels are periodic and most consonants are not, so a vowel range that
 * strays into a burst or a fricative loses that part. A voiced consonant such
 * as m or n is periodic too and is not removed. Off by default, because it
 * runs a waveform analysis over the whole recording. It acts on the audio and
 * region outputs when the selection is VOWELS; records, which describe what
 * was recognised, and the other selections are not changed.
 *
 * A window is voiced when its periodicity, divided by the share of pairs its
 * lag can pair, is at least threshold. A voiced window stands for the hop
 * samples around its centre, so a smaller hop gives finer edges at more cost.
 * The threshold is a starting choice, not a measured one.
 */
struct VoicingGate {
    /** Trim vowel ranges to voiced stretches. */
    bool enabled { false };
    /** Periodicity at or above which a window is voiced. */
    double threshold { 0.5 };
    /** Samples between waveform windows. */
    uint32_t hop { 64 };
};

/**
 * @struct PhonemeExtractorConfig
 * @brief Settings of a PhonemeExtractor.
 *
 * sample_rate states the rate of the input and is refused unless it is 16000,
 * as the networks run at that rate only. speech is passed to the
 * SpeechSegmenter, except that max_run is held to what one call of the phoneme
 * network accepts and measure is ignored. merge_adjacent joins output ranges
 * that touch, for slicing. gate trims vowel ranges to voiced stretches.
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
                FacetMember<&PhonemeExtractorConfig::speech> {},
                FacetMember<&PhonemeExtractorConfig::gate> {},
            });
    }

    /** Which phonemes the output holds, as a PhonemeSelection value. */
    Reflect::EnumByte<PhonemeSelection> method { PhonemeSelection::ALL };
    /** Sample rate in Hz of the input. Only 16000 is accepted. */
    double sample_rate { static_cast<double>(SileroVadModel::sample_rate) };
    /** Join output ranges that touch, so a run of phonemes is one range. */
    bool merge_adjacent { false };
    /** How speech is found. */
    SpeechSettings speech {};
    /** Trims vowel ranges to voiced stretches. */
    VoicingGate gate {};
};

/**
 * @brief The segmenter settings a PhonemeExtractorConfig implies.
 *
 * Runs are capped at HubertPhonemeModel::max_samples so that each speech
 * interval fits one call of the phoneme network, and measures are off.
 */
[[nodiscard]] inline SpeechSegmenterConfig speech_segmenter_config(const PhonemeExtractorConfig& config)
{
    SpeechSegmenterConfig segmenter;
    segmenter.sample_rate = config.sample_rate;
    segmenter.speech = config.speech;
    segmenter.speech.measure = false;
    if (segmenter.speech.max_run == 0 || segmenter.speech.max_run > HubertPhonemeModel::max_samples) {
        segmenter.speech.max_run = HubertPhonemeModel::max_samples;
    }
    return segmenter;
}

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
            FacetMember<&PhonemeExtraction::voiced> {},
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
    /** Voiced stretches of the recording, filled only when the gate trimmed vowels. */
    std::vector<std::pair<size_t, size_t>> voiced;
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
 * @class PhonemeExtractor
 * @brief The phonemes of a whole mono 16 kHz recording, with the pauses between speech taken out.
 *
 * Finds speech with a SpeechSegmenter, then runs the phoneme network once on each
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
        : m_speech(vad_weights, speech_segmenter_config(config))
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
        if (config.gate.enabled && (!std::isfinite(config.gate.threshold) || config.gate.threshold <= 0.0 || config.gate.hop == 0)) {
            return "gate threshold must be finite and greater than 0, and its hop greater than 0";
        }
        return SpeechSegmenter<>::diagnose(speech_segmenter_config(config));
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
                record.speech = m_speech.segment(samples, speech_segmenter_config(cfg)).speech;
                recognise_phonemes(samples, record);
                if (gate_applies(cfg)) {
                    record.voiced = find_voiced(samples, cfg);
                }
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
    static PhonemeAnalyzerConfig phoneme_settings(const PhonemeExtractorConfig& config)
    {
        PhonemeAnalyzerConfig settings;
        settings.method = PhonemeMethod::HELD_CLASSES;
        settings.sample_rate = config.sample_rate;
        return settings;
    }

    static bool gate_applies(const PhonemeExtractorConfig& cfg)
    {
        return cfg.gate.enabled && static_cast<PhonemeSelection>(cfg.method) == PhonemeSelection::VOWELS;
    }

    std::vector<std::pair<size_t, size_t>> find_voiced(std::span<const double> samples, const PhonemeExtractorConfig& cfg)
    {
        if (!m_waveform || m_waveform_hop != cfg.gate.hop) {
            m_waveform.emplace(voicing_analyzer_config(cfg.sample_rate, cfg.gate.hop));
            m_waveform_hop = cfg.gate.hop;
        }
        return voiced_intervals(*m_waveform, samples, cfg.gate.threshold, cfg.gate.hop);
    }

    struct Piece {
        size_t start;
        size_t end;
        size_t row;
    };

    static std::vector<Piece> pieces_of(const PhonemeExtraction& record, const std::vector<size_t>& rows)
    {
        const bool gated = gate_applies(record.config);
        std::vector<Piece> pieces;
        for (const size_t row : rows) {
            const auto& event = record.events[row];
            if (event.end <= event.start) {
                continue;
            }
            if (!gated) {
                pieces.push_back({ event.start, event.end, row });
                continue;
            }
            for (const auto& [start, end] : Kinesis::Discrete::intersect_intervals({ { event.start, event.end } }, record.voiced)) {
                pieces.push_back({ start, end, row });
            }
        }
        return pieces;
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
            for (const auto& piece : pieces_of(record, rows)) {
                const auto& event = record.events[piece.row];
                auto region = Kakshya::Region::audio_span(piece.start, piece.end - 1, 0, 0,
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
            for (const auto& piece : pieces_of(record, rows)) {
                ranges.emplace_back(piece.start, piece.end);
            }
            if (record.config.merge_adjacent) {
                ranges = Kinesis::Discrete::merge_intervals(ranges);
            }
            std::vector<std::vector<double>> channel { Kinesis::Discrete::slice_intervals(samples, ranges) };
            return this->convert_result(channel, info);
        }
    }

    RegionSpeechSegmenter m_speech;
    StandardPhonemeAnalyzer m_phonemes;
    std::optional<StandardWaveformAnalyzer> m_waveform;
    uint32_t m_waveform_hop { 0 };
};

/// Records: one Composite record per selected phoneme
using StandardPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, Kakshya::CompositeArray>;

/// Regions: one region per selected phoneme
using RegionPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, Kakshya::RegionGroup>;

/// Audio: the audio of the selected phonemes, concatenated
using AudioPhonemeExtractor = PhonemeExtractor<std::vector<Kakshya::DataVariant>, std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
