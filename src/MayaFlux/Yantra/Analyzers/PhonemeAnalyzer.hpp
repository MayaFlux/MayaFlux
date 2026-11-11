#pragma once

#include "MayaFlux/Transitive/Reflect/EnumReflect.hpp"
#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Kakshya/Source/TensorContainer.hpp"
#include "MayaFlux/Yantra/Executors/Audio/HubertPhonemeModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file PhonemeAnalyzer.hpp
 * @brief Which phoneme is spoken near each 20 ms frame of an utterance, from a learned network run on the GPU.
 */

namespace MayaFlux::Yantra {

/**
 * @enum PhonemeMethod
 * @brief The per-frame values a PhonemeAnalyzer reports as its output data.
 */
enum class PhonemeMethod : uint8_t {
    POSTERIORS, ///< One column per class: the probability of each of the network's classes
    BROAD_CLASSES ///< Three columns: the summed probability of the vowels, of the consonants, and of everything else
};

/**
 * @struct PhonemeAnalyzerConfig
 * @brief Settings of a PhonemeAnalyzer.
 *
 * The network fixes a frame at 400 samples of 16 kHz audio every 320 samples,
 * so the window and hop of the base config are not settings. sample_rate
 * states the rate of the input and exists so that a wrong one is refused with
 * its reason: the analyzer does not convert.
 */
struct PhonemeAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&PhonemeAnalyzerConfig::method> {},
                FacetMember<&PhonemeAnalyzerConfig::sample_rate> {},
                FacetMember<&PhonemeAnalyzerConfig::keep_embeddings> {},
            });
    }

    PhonemeAnalyzerConfig()
    {
        window_size = HubertPhonemeModel::frame_span;
        hop_size = HubertPhonemeModel::frame_hop;
    }

    /** Values reported as the output data, as a PhonemeMethod value. */
    Reflect::EnumByte<PhonemeMethod> method { PhonemeMethod::POSTERIORS };
    /** Sample rate in Hz of the input. The network runs at 16000 only, so any other value is refused. */
    double sample_rate { static_cast<double>(HubertPhonemeModel::sample_rate) };
    /** Also read back the network's state of every frame. They are large: 768 values per frame. */
    bool keep_embeddings { false };
};

/**
 * @struct PhonemeEvent
 * @brief One phoneme the network recognised.
 */
struct MAYAFLUX_API PhonemeEvent {
    /** Class of the phoneme, 0 to HubertPhonemeModel::phonemes - 1. */
    uint32_t phoneme { 0 };
    /** Frame it was recognised on. */
    size_t frame { 0 };
    /** First sample of that frame. The frame spans 400 samples from there. */
    size_t sample { 0 };
    /** Probability the network gave the phoneme on that frame. */
    float probability { 0.0F };
};

/**
 * @struct PhonemeAnalysis
 * @brief Analysis result of a PhonemeAnalyzer.
 */
struct MAYAFLUX_API PhonemeAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&PhonemeAnalysis::config> {},
            FacetMember<&PhonemeAnalysis::frames> {},
            FacetMember<&PhonemeAnalysis::posteriors> {},
            FacetMember<&PhonemeAnalysis::best> {},
            FacetMember<&PhonemeAnalysis::events> {},
            FacetMember<&PhonemeAnalysis::frame_positions> {},
            FacetMember<&PhonemeAnalysis::embeddings> {},
        };
    }

    /** Settings the analysis was produced under. */
    PhonemeAnalyzerConfig config;
    /** Frames of the utterance. */
    size_t frames { 0 };
    /** Probability of every class on every frame, frame after frame, HubertPhonemeModel::classes values each. */
    std::vector<float> posteriors;
    /** Most probable class of each frame. Mostly the blank class: a phoneme is emitted on one frame. */
    std::vector<uint32_t> best;
    /** The phonemes in order: every frame whose most probable class is a phoneme and differs from the frame before. */
    std::vector<PhonemeEvent> events;
    /** First and one past the last sample of each frame. */
    std::vector<std::pair<size_t, size_t>> frame_positions;
    /** State of every frame before the final layer, HubertPhonemeModel::hidden_width values each. Kept on request. */
    std::vector<float> embeddings;

    /**
     * @brief The probabilities as a frames by classes float32 tensor.
     * @return The container, or nullptr when there are no frames.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TensorContainer> posterior_tensor(std::string name) const
    {
        return as_tensor(posteriors, HubertPhonemeModel::classes, std::move(name));
    }

    /**
     * @brief The states as a frames by hidden_width float32 tensor.
     * @return The container, or nullptr when the states were not kept.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::TensorContainer> embedding_tensor(std::string name) const
    {
        return as_tensor(embeddings, HubertPhonemeModel::hidden_width, std::move(name));
    }

private:
    [[nodiscard]] static std::shared_ptr<Kakshya::TensorContainer> as_tensor(const std::vector<float>& values, uint64_t width, std::string name)
    {
        if (values.empty()) {
            return nullptr;
        }
        auto data = Kakshya::TensorData::from_values<float>(values, { values.size() / width, width }, std::move(name));
        return data ? std::make_shared<Kakshya::TensorContainer>(std::move(*data)) : nullptr;
    }
};

/** @brief Region attribute: the probability the network gave the region's phoneme. */
inline const Facet<double> phoneme_probability {};

/** @brief Region attribute: the class of the region's phoneme. */
inline const Facet<uint32_t> phoneme_class {};

/**
 * @brief One region per recognised phoneme, labelled with its ARPABET symbol.
 *
 * A region covers the frame the phoneme was recognised on, not the phoneme's
 * duration, which the network does not report. Each carries
 * phoneme_probability and phoneme_class.
 *
 * @param analysis The analysis whose events become regions.
 * @param first_channel First channel the regions cover.
 * @param last_channel Last channel the regions cover, inclusive.
 * @param name Name of the group.
 */
[[nodiscard]] inline Kakshya::RegionGroup phoneme_regions(
    const PhonemeAnalysis& analysis, uint32_t first_channel, uint32_t last_channel, std::string name = "phonemes")
{
    Kakshya::RegionGroup group(std::move(name));
    for (const auto& event : analysis.events) {
        auto region = Kakshya::Region::audio_span(event.sample, event.sample + HubertPhonemeModel::frame_span - 1,
            first_channel, last_channel, std::string(HubertPhonemeModel::label(event.phoneme)));
        region.set(phoneme_probability, static_cast<double>(event.probability));
        region.set(phoneme_class, event.phoneme);
        group.add_region(region);
    }
    return group;
}

/**
 * @class PhonemeAnalyzer
 * @brief Which phoneme is spoken near each frame of a mono 16 kHz utterance, from a learned network.
 *
 * Where WaveformAnalyzer measures how a waveform repeats and VadAnalyzer says
 * whether a sound is speech, this says what is being said, as phonemes: every
 * frame is judged with the whole utterance as its context, which no measure of
 * a single window can do.
 *
 * The analyzer is named for the task. Today it runs one network, a HuBERT base
 * fine tuned on read English, recognised by its tensors; weights for any other
 * network are rejected with the reason. Its classes are the 39 ARPABET
 * phonemes, an unknown class, the blank class and two unused markers, named by
 * HubertPhonemeModel::label.
 *
 * The network emits each phoneme on one frame and the blank class between, so
 * the result tells which phonemes occur and near which frames, not where each
 * starts and ends. One call is one utterance of at most 30 s: nothing carries
 * over between calls, and a longer input is refused with the reason. Cut a
 * long recording at its pauses first, for instance with VadAnalyzer and
 * IntervalExtractor, which also keeps the network off everything that is not
 * speech. One channel only.
 *
 * The weights are an explicit dependency: the caller loads the network's
 * tensors and passes them. The network runs in native float on the vulkan
 * target and has no CPU twin. It is not in the default registry, because it
 * cannot be built without its weights; catalog it with a factory to use it in
 * a grammar. It must not run on the audio thread.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class PhonemeAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<PhonemeAnalyzerConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from the network weights.
     * @param weights The tensors of the network, as in hubert_base_phoneme_en.safetensors.
     * @param config Analysis settings.
     * @throws std::invalid_argument When diagnose refuses the config, or the weights are missing a tensor or one has the wrong shape.
     */
    explicit PhonemeAnalyzer(const Kakshya::TensorSet& weights, PhonemeAnalyzerConfig config = {})
    {
        if (const std::string reason = diagnose(config); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "PhonemeAnalyzer: {}", reason);
        }
        if (const std::string reason = HubertPhonemeModel::diagnose(weights); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "PhonemeAnalyzer: {}", reason);
        }
        m_weights = std::make_shared<const Kakshya::TensorSet>(weights);
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    /**
     * @brief Why a config cannot be analysed.
     * @return The reason, empty when the config is usable: its sample_rate is the 16000 Hz the network runs at.
     */
    [[nodiscard]] static std::string diagnose(const PhonemeAnalyzerConfig& config)
    {
        if (config.sample_rate != static_cast<double>(HubertPhonemeModel::sample_rate)) {
            return std::format("sample_rate {} is not the {} Hz the network runs at; convert the input first, for example with Kinesis::Discrete::resample",
                config.sample_rate, HubertPhonemeModel::sample_rate);
        }
        return {};
    }

    /**
     * @brief Type-safe phoneme analysis
     */
    PhonemeAnalysis analyze_phonemes(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<PhonemeAnalysis>(result);
    }

    PhonemeAnalysis analyze_phonemes(const InputType& data)
    {
        return this->analyze_phonemes(input_type { data });
    }

    [[nodiscard]] PhonemeAnalysis get_phoneme_analysis() const
    {
        return safe_any_cast_or_throw<PhonemeAnalysis>(this->get_current_analysis());
    }

    /**
     * @brief Append to a recording the caller shares with other work.
     *
     * Call it before the first analysis. The recording must be compute
     * isolated.
     *
     * @throws std::logic_error when the network has already been created.
     */
    void share_recording(std::shared_ptr<GpuDispatchRecorder> recording)
    {
        if (m_model) {
            error<std::logic_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "The recording can only be shared before the network starts");
        }
        m_recording = std::move(recording);
    }

    /**
     * @brief Where the states of the latest utterance are on the device, for a consumer on the same recording.
     */
    [[nodiscard]] HubertPhonemeModel::DeviceBuffer hidden_buffer()
    {
        return network().hidden_buffer();
    }

    /**
     * @brief Where the probabilities of the latest utterance are on the device, for a consumer on the same recording.
     */
    [[nodiscard]] HubertPhonemeModel::DeviceBuffer posterior_buffer()
    {
        return network().posterior_buffer();
    }

    /** @brief Frames the latest utterance left on the device. */
    [[nodiscard]] size_t resident_frames() const noexcept
    {
        return m_model ? m_model->resident_frames() : 0;
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::PATTERN;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "PhonemeAnalyzer";
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
            const auto& cfg = this->template config<PhonemeAnalyzerConfig>();
            if (const std::string reason = diagnose(cfg); !reason.empty()) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "PhonemeAnalyzer: {}", reason);
            }

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            if (data_span.size() != 1) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "PhonemeAnalyzer analyses one channel and was given {}", data_span.size());
            }

            const std::vector<float> samples(data_span[0].begin(), data_span[0].end());

            constexpr size_t classes = HubertPhonemeModel::classes;
            PhonemeAnalysis analysis;
            analysis.config = cfg;
            analysis.posteriors = network().process(samples);
            analysis.frames = analysis.posteriors.size() / classes;
            if (cfg.keep_embeddings) {
                analysis.embeddings = network().hidden_states();
            }
            describe_frames(analysis);

            this->store_current_analysis(analysis);

            auto channel_values = select_values(analysis, static_cast<PhonemeMethod>(cfg.method));

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Phoneme analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    HubertPhonemeModel& network() const
    {
        if (!m_model) {
            m_model = std::make_unique<HubertPhonemeModel>(*m_weights, m_recording);
        }
        return *m_model;
    }

    static void describe_frames(PhonemeAnalysis& analysis)
    {
        constexpr size_t classes = HubertPhonemeModel::classes;
        analysis.best.reserve(analysis.frames);
        analysis.frame_positions.reserve(analysis.frames);
        uint32_t previous = HubertPhonemeModel::blank_class;
        for (size_t frame = 0; frame < analysis.frames; ++frame) {
            const float* row = analysis.posteriors.data() + frame * classes;
            const auto top = static_cast<uint32_t>(std::max_element(row, row + classes) - row);
            const size_t start = frame * HubertPhonemeModel::frame_hop;
            analysis.best.push_back(top);
            analysis.frame_positions.emplace_back(start, start + HubertPhonemeModel::frame_span);
            if (top != previous && top < HubertPhonemeModel::phonemes) {
                analysis.events.push_back({ .phoneme = top, .frame = frame, .sample = start, .probability = row[top] });
            }
            previous = top;
        }
    }

    static std::vector<std::vector<double>> select_values(const PhonemeAnalysis& analysis, PhonemeMethod method)
    {
        constexpr size_t classes = HubertPhonemeModel::classes;
        const bool broad = method == PhonemeMethod::BROAD_CLASSES;
        std::vector<std::vector<double>> columns(broad ? 3 : classes, std::vector<double>(analysis.frames, 0.0));
        for (size_t frame = 0; frame < analysis.frames; ++frame) {
            const float* row = analysis.posteriors.data() + frame * classes;
            for (uint32_t index = 0; index < classes; ++index) {
                if (!broad) {
                    columns[index][frame] = row[index];
                } else if (index >= HubertPhonemeModel::phonemes) {
                    columns[2][frame] += row[index];
                } else {
                    columns[HubertPhonemeModel::is_vowel(index) ? 0 : 1][frame] += row[index];
                }
            }
        }
        return columns;
    }

    std::shared_ptr<const Kakshya::TensorSet> m_weights;
    std::shared_ptr<GpuDispatchRecorder> m_recording;
    mutable std::unique_ptr<HubertPhonemeModel> m_model;
};

/// Standard phoneme analyzer: DataVariant -> MatrixXd
using StandardPhonemeAnalyzer = PhonemeAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container phoneme analyzer: SignalContainer -> MatrixXd
using ContainerPhonemeAnalyzer = PhonemeAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
