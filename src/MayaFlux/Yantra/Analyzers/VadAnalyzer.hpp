#pragma once

#include "MayaFlux/Yantra/OperationSpec/OperationHelper.hpp"

#include "Analyzer.hpp"
#include <Eigen/Dense>

#include "MayaFlux/Yantra/Executors/Audio/SileroVadModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

/**
 * @file VadAnalyzer.hpp
 * @brief Voice activity probability per 32 ms chunk from a learned network, run on the GPU.
 */

namespace MayaFlux::Yantra {

/**
 * @struct VadAnalyzerConfig
 * @brief Settings of a VadAnalyzer.
 *
 * The network fixes the chunk at 512 samples of 16 kHz audio, so the window and
 * hop of the base config are both 512 and are not settings. sample_rate states
 * the rate of the input and exists so that a wrong one is refused with its
 * reason instead of giving wrong probabilities: the analyzer does not convert.
 * Input at another rate is converted first, for example with
 * Kinesis::Discrete::Resampler.
 */
struct VadAnalyzerConfig : AnalyzerConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            AnalyzerConfig::describe_facets(),
            std::tuple {
                FacetMember<&VadAnalyzerConfig::flush_tail> {},
                FacetMember<&VadAnalyzerConfig::sample_rate> {},
                FacetMember<&VadAnalyzerConfig::keep_embeddings> {},
            });
    }

    VadAnalyzerConfig()
    {
        window_size = SileroVadModel::chunk_samples;
        hop_size = SileroVadModel::chunk_samples;
    }

    /** Also run the partial chunk left at the end of each call, padded with zeros. Off for a stream, on for a whole clip. */
    bool flush_tail { false };
    /** Sample rate in Hz of the input. The network runs at 16000 only, so any other value is refused. */
    double sample_rate { static_cast<double>(SileroVadModel::sample_rate) };
    /** Also read back the network's recurrent output of every chunk, SileroVadModel::hidden_width values each. */
    bool keep_embeddings { false };
};

/**
 * @struct VadAnalysis
 * @brief Analysis result of a VadAnalyzer.
 */
struct MAYAFLUX_API VadAnalysis {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&VadAnalysis::config> {},
            FacetMember<&VadAnalysis::probabilities> {},
            FacetMember<&VadAnalysis::chunk_positions> {},
            FacetMember<&VadAnalysis::embeddings> {},
        };
    }

    /** Settings the analysis was produced under. */
    VadAnalyzerConfig config;
    /** Speech probability of each chunk completed by this call, 0 to 1. */
    std::vector<float> probabilities;
    /** First and one past the last sample of each chunk, counted from the start of the stream. */
    std::vector<std::pair<size_t, size_t>> chunk_positions;
    /**
     * Recurrent output of the network for each chunk, SileroVadModel::hidden_width values each, kept on request.
     * It is what the probability is derived from; the network was trained only to tell speech from other sound.
     */
    std::vector<float> embeddings;
};

/**
 * @class VadAnalyzer
 * @brief Whether each 32 ms chunk of a mono 16 kHz stream is voice activity, from a learned network.
 *
 * Where WaveformAnalyzer measures the shape of the waveform, this says what
 * the sound is: a sung note and a fan hum are as periodic as a vowel, and only
 * a learned model separates them from speech. The probabilities are the
 * network's own, before any thresholding.
 *
 * The analyzer is named for the task. Today it runs one network, Silero VAD at
 * 16 kHz, recognised by its tensors; weights for any other network are
 * rejected with the reason.
 *
 * The analyzer is a stream. The recurrent state, the 64 samples of context
 * before the next chunk and any partial chunk persist between calls, so
 * successive calls continue one signal. Call reset for a new one. For a whole
 * clip, set flush_tail so the final partial chunk is run too. One channel only.
 *
 * The weights are an explicit dependency: the caller loads the network's
 * tensors and passes them. The network runs in native float on the vulkan
 * target and has no CPU twin. It is not registered with the default registry,
 * because it has no selection to resolve by, and its result arrives a chunk
 * late, so it must not run on the audio thread.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>, ComputeData OutputType = Eigen::MatrixXd>
class VadAnalyzer : public Analyzer<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using base_type = Analyzer<InputType, OutputType>;
    using supports = Supports<VadAnalyzerConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Construct from the network weights.
     * @param weights The tensors of the network. Silero VAD at 16 kHz is the one supported, as in silero_vad_16k.safetensors.
     * @param config Analysis settings.
     * @throws std::invalid_argument When diagnose refuses the config, or the weights are missing a tensor or one has the wrong shape.
     */
    explicit VadAnalyzer(const Kakshya::TensorSet& weights, VadAnalyzerConfig config = {})
    {
        if (const std::string reason = diagnose(config); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "VadAnalyzer: {}", reason);
        }
        if (const std::string reason = SileroVadModel::diagnose(weights); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "VadAnalyzer: {}", reason);
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
    [[nodiscard]] static std::string diagnose(const VadAnalyzerConfig& config)
    {
        if (config.sample_rate != static_cast<double>(SileroVadModel::sample_rate)) {
            return std::format("sample_rate {} is not the {} Hz the network runs at; convert the input first, for example with Kinesis::Discrete::Resampler",
                config.sample_rate, SileroVadModel::sample_rate);
        }
        return {};
    }

    /**
     * @brief Type-safe speech probability analysis
     */
    VadAnalysis analyze_speech(const input_type& data)
    {
        auto result = this->analyze_data(data);
        return safe_any_cast_or_throw<VadAnalysis>(result);
    }

    VadAnalysis analyze_speech(const InputType& data)
    {
        return this->analyze_speech(input_type { data });
    }

    [[nodiscard]] VadAnalysis get_speech_analysis() const
    {
        return safe_any_cast_or_throw<VadAnalysis>(this->get_current_analysis());
    }

    /**
     * @brief Append to a recording the caller shares with other work.
     *
     * With a shared recording the network goes to the device in the same
     * submission as the work recorded around it, and analyze_speech submits
     * and waits for all of it. Call it before the first analysis or record. The
     * recording must be compute isolated.
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
     * @brief Run every chunk the samples complete without waiting for the result.
     *
     * The probabilities stay on the device, in probability_buffer, and are
     * recorded into the analyzer's recording without a submission. A consumer
     * on the same recording, such as an IntervalExtractor, reads them in place
     * and submits. Whole chunks only: a partial chunk stays pending, and
     * flush_tail does not apply. At most 4096 chunks run per call, so read the
     * buffer before the next call.
     *
     * @param samples Mono samples at 16 kHz, continuing the stream.
     * @return The number of chunks recorded.
     * @throws std::invalid_argument when the config cannot be analysed.
     */
    [[nodiscard]] size_t record(std::span<const float> samples)
    {
        const auto& cfg = this->template config<VadAnalyzerConfig>();
        if (const std::string reason = diagnose(cfg); !reason.empty()) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "VadAnalyzer: {}", reason);
        }
        const size_t chunks = network().record(samples);
        m_chunks_done += chunks;
        return chunks;
    }

    /**
     * @brief Where the probabilities of the latest run are on the device, for a consumer on the same recording.
     */
    [[nodiscard]] SileroVadModel::ProbabilityBuffer probability_buffer()
    {
        return network().probability_buffer();
    }

    /**
     * @brief Where the network's recurrent output of the latest run is on the device, SileroVadModel::hidden_width values per chunk.
     */
    [[nodiscard]] SileroVadModel::ProbabilityBuffer hidden_buffer()
    {
        return network().hidden_buffer();
    }

    /** @brief Probabilities the latest run left in probability_buffer. */
    [[nodiscard]] size_t resident_count() const noexcept
    {
        return m_model ? m_model->resident_count() : 0;
    }

    /**
     * @brief Start a new signal: forget the recurrent state, the context and any partial chunk.
     */
    void reset()
    {
        if (m_model) {
            m_model->reset();
        }
        m_chunks_done = 0;
    }

    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::CUSTOM;
    }

    [[nodiscard]] std::string get_name() const override
    {
        return "VadAnalyzer";
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
            const auto& cfg = this->template config<VadAnalyzerConfig>();
            if (const std::string reason = diagnose(cfg); !reason.empty()) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "VadAnalyzer: {}", reason);
            }

            auto [data_span, structure_info] = OperationHelper::extract_structured_double(
                const_cast<input_type&>(input));

            if (data_span.size() != 1) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "VadAnalyzer analyses one channel and was given {}", data_span.size());
            }

            const std::vector<float> samples(data_span[0].begin(), data_span[0].end());

            VadAnalysis analysis;
            analysis.config = cfg;
            network().keep_hidden(cfg.keep_embeddings);
            analysis.probabilities = network().process(samples);
            if (cfg.flush_tail) {
                const auto tail = network().flush();
                analysis.probabilities.insert(analysis.probabilities.end(), tail.begin(), tail.end());
            }
            analysis.embeddings = network().take_hidden();

            analysis.chunk_positions.reserve(analysis.probabilities.size());
            for (size_t i = 0; i < analysis.probabilities.size(); ++i) {
                const size_t start = (m_chunks_done + i) * SileroVadModel::chunk_samples;
                analysis.chunk_positions.emplace_back(start, start + SileroVadModel::chunk_samples);
            }
            m_chunks_done += analysis.probabilities.size();

            this->store_current_analysis(analysis);

            std::vector<std::vector<double>> channel_values(1);
            channel_values[0].assign(analysis.probabilities.begin(), analysis.probabilities.end());

            output_type output = this->convert_result(channel_values, structure_info);
            output.metadata = input.metadata;
            output.facets = input.facets;
            output.facets.set(analysis);
            return output;
        } catch (const std::exception& e) {
            MF_ERROR(Journal::Component::Yantra, Journal::Context::ComputeMatrix, "Voice activity analysis failed: {}", e.what());
            output_type error_result;
            error_result.metadata = input.metadata;
            error_result.facets = input.facets;
            return error_result;
        }
    }

private:
    SileroVadModel& network() const
    {
        if (!m_model) {
            m_model = std::make_unique<SileroVadModel>(*m_weights, m_recording);
        }
        return *m_model;
    }

    std::shared_ptr<const Kakshya::TensorSet> m_weights;
    std::shared_ptr<GpuDispatchRecorder> m_recording;
    mutable std::unique_ptr<SileroVadModel> m_model;
    size_t m_chunks_done { 0 };
};

/// Standard VAD analyzer: DataVariant -> MatrixXd
using StandardVadAnalyzer = VadAnalyzer<std::vector<Kakshya::DataVariant>, Eigen::MatrixXd>;

/// Container VAD analyzer: SignalContainer -> MatrixXd
using ContainerVadAnalyzer = VadAnalyzer<std::shared_ptr<Kakshya::SignalSourceContainer>, Eigen::MatrixXd>;

} // namespace MayaFlux::Yantra
